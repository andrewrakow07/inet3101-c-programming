/* server.c - receives usage from the metering agent over TCP */

/*
 * Messages are one line of text each:
 *   USAGE <account_id> SERVER <cpu_hours> <memory_gb_hours>
 *   USAGE <account_id> STORAGE <gb>
 *   USAGE <account_id> NETWORK <inbound_mb> <outbound_mb>
 *   QUIT
 * Replies are: OK <usage_id>, ERR <reason>, or BYE.
 * The server listens on 127.0.0.1 and takes one agent connection.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "billing.h"

#define LINE_SIZE 256
#define WAIT_FOR_AGENT 60
#define WAIT_FOR_LINE 10

#define LINE_REJECTED 0
#define LINE_ACCEPTED 1
#define LINE_QUIT 2

/* Send text to the agent. */
static void send_text(int fd, char *text)
{
    if (write(fd, text, strlen(text)) < 0) {
        perror("write");
    }
}

/* Handle one line from the agent. The reply is put in reply. */
static int handle_line(char *line, char *reply, int reply_size)
{
    char command[16];
    char resource_text[16];
    int account_id = 0;
    int resource;
    int fields;
    double v1 = 0;
    double v2 = 0;
    Account account;
    UsageRecord usage;
    int result;

    if (sscanf(line, "%15s", command) == 1 && strcmp(command, "QUIT") == 0) {
        snprintf(reply, reply_size, "BYE\n");
        return LINE_QUIT;
    }

    fields = sscanf(line, "%15s %d %15s %lf %lf", command, &account_id, resource_text, &v1, &v2);
    if (fields < 4 || strcmp(command, "USAGE") != 0) {
        snprintf(reply, reply_size, "ERR bad format\n");
        return LINE_REJECTED;
    }

    resource = resource_from_name(resource_text);
    if (resource == 0) {
        snprintf(reply, reply_size, "ERR unknown resource\n");
        return LINE_REJECTED;
    }

    /* storage needs one value, the others need two */
    if (resource != RES_STORAGE && fields != 5) {
        snprintf(reply, reply_size, "ERR missing value\n");
        return LINE_REJECTED;
    }

    if (!(v1 >= 0 && v1 <= MAX_VALUE && v2 >= 0 && v2 <= MAX_VALUE)) {
        snprintf(reply, reply_size, "ERR value out of range\n");
        return LINE_REJECTED;
    }

    result = db_account_get(account_id, &account);
    if (result == 1) {
        snprintf(reply, reply_size, "ERR unknown account\n");
        return LINE_REJECTED;
    }
    if (result < 0) {
        snprintf(reply, reply_size, "ERR database\n");
        return LINE_REJECTED;
    }

    usage_fill(&usage, resource, v1, v2);
    usage.account_id = account_id;
    usage.timestamp = time(NULL);

    if (db_usage_create(&usage) != 0) {
        snprintf(reply, reply_size, "ERR database\n");
        return LINE_REJECTED;
    }

    snprintf(reply, reply_size, "OK %d\n", usage.id);
    return LINE_ACCEPTED;
}

/* Wait for one agent and store the usage it sends.
   Returns 0 when done (also on timeout), -1 on a socket error. */
int ingest_run(int port)
{
    int listen_fd;
    int client_fd;
    int yes = 1;
    int ready;
    int result;
    int accepted = 0;
    int rejected = 0;
    int c;
    int len;
    struct sockaddr_in addr;
    struct timeval timeout;
    fd_set fds;
    FILE *in;
    char line[LINE_SIZE];
    char reply[64];

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }
    if (listen(listen_fd, 1) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    printf("Listening on 127.0.0.1:%d (waiting up to %d seconds)...\n", port, WAIT_FOR_AGENT);
    log_event("ingest: listening on port %d", port);

    /* wait until an agent connects */
    FD_ZERO(&fds);
    FD_SET(listen_fd, &fds);
    timeout.tv_sec = WAIT_FOR_AGENT;
    timeout.tv_usec = 0;
    ready = select(listen_fd + 1, &fds, NULL, NULL, &timeout);
    if (ready <= 0) {
        close(listen_fd);
        if (ready == 0) {
            printf("No agent connected. Back to the menu.\n");
            return 0;
        }
        perror("select");
        return -1;
    }

    client_fd = accept(listen_fd, NULL, NULL);
    close(listen_fd);
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }
    printf("Agent connected.\n");

    /* do not wait forever for the next line */
    timeout.tv_sec = WAIT_FOR_LINE;
    timeout.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    in = fdopen(client_fd, "r");
    if (in == NULL) {
        close(client_fd);
        return -1;
    }

    while (fgets(line, LINE_SIZE, in) != NULL) {
        len = strlen(line);

        if (len == LINE_SIZE - 1 && line[len - 1] != '\n') {
            /* line too long, throw away the rest */
            while ((c = fgetc(in)) != '\n' && c != EOF) {
            }
            snprintf(reply, sizeof(reply), "ERR line too long\n");
            result = LINE_REJECTED;
        } else {
            result = handle_line(line, reply, sizeof(reply));
        }

        send_text(client_fd, reply);

        if (result == LINE_ACCEPTED) {
            accepted++;
        } else if (result == LINE_REJECTED) {
            rejected++;
        } else {
            break;
        }
    }

    fclose(in);
    printf("Ingest finished: %d stored, %d rejected.\n", accepted, rejected);
    log_event("ingest: finished, stored=%d rejected=%d", accepted, rejected);
    return 0;
}
