#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(int argc, char const* arg[])
{
    // ask the user for the file name
    char fileName[256];
    printf("Enter file name: ");
    if (fgets(fileName, sizeof(fileName), stdin) == NULL) {
        printf("Error: Could not read file name.\n");
        return 1;
    }
    fileName[strcspn(fileName, "\n")] = '\0';

    if (strlen(fileName) == 0) {
        printf("Error: File name is empty.\n");
        return 1;
    }

    // create client socket
    int serverSocketID = socket(AF_INET, SOCK_STREAM, 0);

    // define server address
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9001);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    int connectStatus = connect(serverSocketID, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (connectStatus == -1) {
        printf("Error: Could not connect to the server. \n");
        return 1;
    }

    // send the file name to the server
    send(serverSocketID, fileName, strlen(fileName), 0);

    // receive the response
    char serverMsg[2100];
    int total = 0;
    int n;
    while ((n = recv(serverSocketID, serverMsg + total, sizeof(serverMsg) - 1 - total, 0)) > 0) {
        total += n;
    }
    serverMsg[total] = '\0';
    close(serverSocketID);

    if (total == 0) {
        printf("Error: No response from the server.\n");
        return 1;
    }

    // print the file contents or the error message
    if (strncmp(serverMsg, "OK\n", 3) == 0) {
        printf("%s", serverMsg + 3);
    }
    else {
        printf("%s", serverMsg);
    }

    return 0;
}
