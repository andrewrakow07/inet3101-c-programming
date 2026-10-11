#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define MAX_NAME 255

int main(int argc, char *argv[])
{
    const char *ip = "127.0.0.1";
    char filename[MAX_NAME + 2];

    if (argc >= 2)
    {
        ip = argv[1];
    }
    if (argc >= 3)
    {
        if (strlen(argv[2]) > MAX_NAME)
        {
            fprintf(stderr, "Error: file name too long\n");
            return 1;
        }
        strcpy(filename, argv[2]);
    }
    else
    {
        printf("Enter file name: ");
        if (!fgets(filename, sizeof(filename), stdin))
        {
            fprintf(stderr, "Error: could not read file name\n");
            return 1;
        }
        filename[strcspn(filename, "\r\n")] = '\0';
    }

    if (filename[0] == '\0')
    {
        fprintf(stderr, "Error: file name is empty\n");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
    {
        fprintf(stderr, "Error: invalid server address\n");
        close(fd);
        return 1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        close(fd);
        return 1;
    }

    if (send(fd, filename, strlen(filename), 0) < 0)
    {
        perror("send");
        close(fd);
        return 1;
    }

    char buf[2048 + 64];
    size_t total = 0;
    ssize_t n;
    while ((n = recv(fd, buf + total, sizeof(buf) - 1 - total, 0)) > 0)
    {
        total += (size_t)n;
        if (total >= sizeof(buf) - 1)
            break;
    }
    buf[total] = '\0';
    close(fd);

    if (total == 0)
    {
        fprintf(stderr, "Error: no response from server\n");
        return 1;
    }

    if (strncmp(buf, "OK\n", 3) == 0)
    {
        printf("%s", buf + 3);
        return 0;
    }

    fprintf(stderr, "%s", buf);
    return 1;
}
