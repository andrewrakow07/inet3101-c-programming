#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
 
int serverSocketID;
 
// runs when the server gets Ctrl+C or a kill signal
void handleSignal(int sig)
{
    printf("\nShutting down server gracefully...\n");
    close(serverSocketID);
    exit(0);
}
 
int main(int argc, char const* arg[])
{
    // set up signal handling for graceful termination
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
 
    // create server socket
    serverSocketID = socket(AF_INET, SOCK_STREAM, 0);
 
    // define server address
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9001);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
 
    // bind socket to the specified port and IP
    bind(serverSocketID, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
 
    printf("Listening for Connections\n");
    listen(serverSocketID, 5);
 
    // infinite loop so the server keeps accepting clients
    while (1) {
        printf("Waiting for next request from Client\n");
        int clientSocket = accept(serverSocketID, NULL, NULL);
 
        // read the file name sent by the client
        char fileName[256];
        int len = recv(clientSocket, fileName, sizeof(fileName) - 1, 0);
        if (len <= 0) {
            close(clientSocket);
            continue;
        }
        fileName[len] = '\0';
 
        // check for an invalid file name
        if (strlen(fileName) == 0 || strchr(fileName, '/') != NULL || strstr(fileName, "..") != NULL) {
            char msg[] = "ERROR: Invalid file name\n";
            send(clientSocket, msg, strlen(msg), 0);
            printf("Invalid file name requested\n");
            close(clientSocket);
            continue;
        }
 
        // check if the file exists
        FILE *file = fopen(fileName, "r");
        if (file == NULL) {
            char msg[] = "ERROR: File not found\n";
            send(clientSocket, msg, strlen(msg), 0);
            printf("File not found: %s\n", fileName);
            close(clientSocket);
            continue;
        }
 
        // read up to 2049 characters so we can tell if the file is too big
        char fileData[2050];
        int size = fread(fileData, 1, 2049, file);
        fclose(file);
 
        if (size > 2048) {
            char msg[] = "ERROR: File size exceeds 2048 characters\n";
            send(clientSocket, msg, strlen(msg), 0);
            printf("File too large: %s\n", fileName);
            close(clientSocket);
            continue;
        }
 
        // send OK followed by the file contents
        send(clientSocket, "OK\n", 3, 0);
        send(clientSocket, fileData, size, 0);
        printf("Sent file: %s\n", fileName);
 
        close(clientSocket);
    }
 
    return 0;
}
