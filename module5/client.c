#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define MAX_FILE 2048
#define MAX_NAME 255

static volatile sig_atomic_t running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}

static void send_all(int fd, const char *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return;
        }
        sent += (size_t)n;
    }
}

static void send_error(int fd, const char *msg)
{
    char out[256];
    int n = snprintf(out, sizeof(out), "ERROR: %s\n", msg);
    send_all(fd, out, (size_t)n);
}

static int valid_name(const char *name)
{
    if (name[0] == '\0')
        return 0;
    if (strstr(name, "..") != NULL)
        return 0;
    for (const char *p = name; *p; p++)
    {
        if (*p == '/' || *p == '\\')
            return 0;
    }
    return 1;
}

static void handle_client(int fd)
{
    char name[MAX_NAME + 2];
    ssize_t n = recv(fd, name, MAX_NAME + 1, 0);
    if (n <= 0)
        return;
    if (n > MAX_NAME)
    {
        send_error(fd, "Invalid file name (too long)");
        return;
    }
    name[n] = '\0';
    name[strcspn(name, "\r\n")] = '\0';

    if (!valid_name(name))
    {
        send_error(fd, "Invalid file name");
        return;
    }

    FILE *f = fopen(name, "r");
    if (!f)
    {
        send_error(fd, "File not found");
        return;
    }

    char data[MAX_FILE + 1];
    size_t len = fread(data, 1, MAX_FILE + 1, f);
    fclose(f);

    if (len > MAX_FILE)
    {
        send_error(fd, "File size exceeds 2048 characters");
        return;
    }

    send_all(fd, "OK\n", 3);
    send_all(fd, data, len);
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* no SA_RESTART so accept() returns on signal */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d (Ctrl+C to stop)\n", PORT);

    while (running)
    {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int client_fd = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            perror("accept");
            break;
        }
        printf("Client connected: %s\n", inet_ntoa(caddr.sin_addr));
        handle_client(client_fd);
        close(client_fd);
    }

    printf("\nShutting down server gracefully...\n");
    close(server_fd);
    return 0;
}
