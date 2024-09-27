#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

struct childargs { // child process arguments
    int socketfd;
    struct sockaddr* client_addr;
    int client_len;
};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void* sendMessage(void* args) {
    char buffer[255];
    bzero(buffer, sizeof(buffer));
    struct childargs* client_args = (struct childargs*)args;

    strcpy(buffer, "Message sent from the server - Hello");
    sendto(client_args->socketfd, buffer, sizeof(buffer), 0, client_args->client_addr, client_args->client_len);
}

int main() {
    int serverPort = 67;
    struct sockaddr_in server_addr, client_addr;
    socklen_t clientLength;

    int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        error("Error Opening socket");
    
    bzero((char *) &server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(serverPort);

    if (bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        error("Error trying to bind the server");

    clientLength = sizeof(client_addr);

    while (1) {
        char buffer[255]; // Initialize buffer for messages

        struct childargs th_arg; // Defines an struct for arguments to the child function
        th_arg.client_len = clientLength;
        th_arg.client_addr = (struct sockaddr*) &client_addr;
        th_arg.socketfd = socketfd;

        bzero(buffer, 255); // Fill all the bytes in the buffer with zeros
        recvfrom(socketfd, buffer, 255, 0, (struct sockaddr*) &client_addr, &clientLength);
        printf("Message recieved: %s", buffer);

        pthread_t child_th; // Create a child thread

        if (pthread_create(&child_th, NULL, &sendMessage, &th_arg) != 0)
            error("There was an error creating a child process.");
        
    }

    close(socketfd);
    return 0;
}