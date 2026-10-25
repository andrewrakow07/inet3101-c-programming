/* meter_agent.c - mock metering agent */

/*
 * Sends random usage readings for one account to the billing system.
 * Usage: ./meter_agent <port> <account_id> <count>
 * Exit codes: 0 ok, 1 bad arguments, 4 cannot connect, 5 server closed early
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* Random number between low and high. */
static double random_between(double low, double high)
{
    return low + (high - low) * ((double)rand() / RAND_MAX);
}

/* Turn text into a number between min and max. Returns -1 if not valid. */
static long read_number(char *text, long min, long max)
{
    char *end;
    long value = strtol(text, &end, 10);

    if (text[0] == '\0' || *end != '\0' || value < min || value > max) {
        return -1;
    }
    return value;
}

int main(int argc, char **argv)
{
    long port, account, count, i;
    int fd;
    int kind;
    struct sockaddr_in addr;
    FILE *in;
    char message[128];
    char reply[64];

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <account_id> <count>\n", argv[0]);
        return 1;
    }

    port = read_number(argv[1], 1024, 65535);
    account = read_number(argv[2], 1, 1000000000);
    count = read_number(argv[3], 1, 1000);
    if (port < 0 || account < 0 || count < 0) {
        fprintf(stderr, "Bad argument. Port 1024-65535, account_id 1 or more, count 1-1000.\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL) ^ getpid());

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 4;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Cannot connect on port %ld. Is menu option 9 running?\n", port);
        close(fd);
        return 4;
    }

    in = fdopen(fd, "r");
    if (in == NULL) {
        close(fd);
        return 4;
    }

    for (i = 0; i < count; i++) {
        kind = rand() % 3;

        if (kind == 0) {
            snprintf(message, sizeof(message), "USAGE %ld SERVER %.2f %.2f\n",
                     account, random_between(1, 48), random_between(4, 256));
        } else if (kind == 1) {
            snprintf(message, sizeof(message), "USAGE %ld STORAGE %.2f\n",
                     account, random_between(10, 500));
        } else {
            snprintf(message, sizeof(message), "USAGE %ld NETWORK %.2f %.2f\n",
                     account, random_between(0, 5000), random_between(0, 5000));
        }

        if (write(fd, message, strlen(message)) < 0) {
            perror("write");
            fclose(in);
            return 5;
        }

        if (fgets(reply, sizeof(reply), in) == NULL) {
            fprintf(stderr, "Server closed the connection early.\n");
            fclose(in);
            return 5;
        }

        message[strcspn(message, "\n")] = '\0';
        reply[strcspn(reply, "\n")] = '\0';
        printf("sent: %-45s reply: %s\n", message, reply);
    }

    if (write(fd, "QUIT\n", 5) < 0) {
        perror("write");
    }
    if (fgets(reply, sizeof(reply), in) != NULL) {
        printf("done (%ld readings sent)\n", count);
    }

    fclose(in);
    return 0;
}
