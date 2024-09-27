#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>

struct client_data { // child process arguments
    int socketfd;
    struct sockaddr* client_addr;
    int client_len;
};

struct DHCP_offer_args {
    u_int8_t op;
    u_int8_t htype;
    u_int8_t hlen;
    u_int8_t hops;
    u_int32_t xid;
    u_int16_t secs;
    u_int16_t flags;
    u_int32_t ciaddr;
    u_int32_t yiaddr;
    u_int32_t siaddr;
    u_int32_t giaddr;
    char chaddr[16];
    char sname[64];
};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void* DHCPOffer(void* args) {
    char buffer[255];
    char hostbuffer[255];
    bzero(buffer, sizeof(buffer));
    
    struct client_data* client_args = (struct client_data*)args;

    if(gethostname(hostbuffer, sizeof(hostbuffer)) < 0)
        perror("There was an error retrieving hostname");

    strcpy(buffer, "Message sent from the server. My name is ");
    strcat(buffer, hostbuffer);
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
        char buffer[576]; // Initialize buffer for messages

        struct client_data th_arg; // Defines an struct for arguments to the child function
        th_arg.client_len = clientLength;
        th_arg.client_addr = (struct sockaddr*) &client_addr;
        th_arg.socketfd = socketfd;

        bzero(buffer, 576); // Fill all the bytes in the buffer with zeros
        recvfrom(socketfd, buffer, 576, 0, (struct sockaddr*) &client_addr, &clientLength);
        buffer[576] = '\0';

        u_int8_t op = buffer[0];
        printf("Received message: %u\n", op);

        pthread_t child_th; // Create a child thread

        if (pthread_create(&child_th, NULL, &DHCPOffer, &th_arg) != 0)
            error("There was an error creating a child process.");
    }

    close(socketfd);
    return 0;
}