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
    char buffer[576];
};

struct DHCP_offer_args {
    u_int32_t offered_addr;

};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

uint32_t get_nbyte_number(char* buffer, int offset, int bytes) {
    uint32_t number = 0; // accumulator for final number
    int current_offset = offset; // byte position in buffer where number starts

    // Loop until you checked all corresponding bytes
    for (int i = bytes; i > 0; i--) {
        /* The number is created by doing a sum of every byte inside the buffer
           Every byte represents a value from 0 to 255 (that's why it's an
           unsigned char), and they are converted to an unsigned int of 32 bit
           (4 bytes). Since each byte it's just 8 bits, they are always put in the
           right-most side of the number in binary. Since we are dealing with Big Endian,
           the most significant bit it's always the first element in pos A and not A+size.
           So, we shift the current byte to its corresponding position, taking in count
           that it always starts at the first 8 bits or first byte of the 32 bit number
           So, if the number is 4 bytes long, buffer[i] << 8*(4-1) shifts its value to
           the most significant position in a 4 byte number (24 + 8), and so on.
        */
        number += (uint32_t)(unsigned char)buffer[current_offset] << (8*(i - 1));
        current_offset += 1;
    }
    return number;
}

void print_packet(char* buffer, int size) {
    for (int i = 0; i < size; i++) {
        printf("%02x ", (u_int8_t)buffer[i]);
    }
}

void* DHCPOffer(void* args) {
    struct client_data client_args = *(struct client_data*)args;
    
    uint8_t op = client_args.buffer[0];
    uint8_t htype = client_args.buffer[1];
    uint8_t hlen = client_args.buffer[2];
    uint8_t hops = client_args.buffer[3];
    uint32_t xid = get_nbyte_number(client_args.buffer, 4, 4);
    uint16_t secs = ntohl(*(uint16_t*)&client_args.buffer[8]);
    uint16_t flags = ntohl(*(uint16_t*)&client_args.buffer[10]);
    uint32_t ciaddr = ntohl(*(uint32_t*)&client_args.buffer[12]);
    uint32_t yiaddr = ntohl(*(uint32_t*)&client_args.buffer[16]);
    uint32_t siaddr = ntohl(*(uint32_t*)&client_args.buffer[20]);
    uint32_t giaddr = ntohl(*(uint32_t*)&client_args.buffer[24]);
    char chaddr[16];

    strcpy(chaddr, &client_args.buffer[28]);

    char buffer[255];
    bzero(buffer, sizeof(buffer));

    sendto(client_args.socketfd, buffer, sizeof(buffer), 0, client_args.client_addr, client_args.client_len);
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
        bzero(buffer, 576); // Fill all the bytes in the buffer with zeros

        recvfrom(socketfd, buffer, 576, 0, (struct sockaddr*) &client_addr, &clientLength);

        struct client_data th_arg; // Defines an struct for arguments to the child function
        th_arg.client_len = clientLength;
        th_arg.client_addr = (struct sockaddr*) &client_addr;
        th_arg.socketfd = socketfd;
        strcpy(th_arg.buffer, buffer);

        pthread_t child_th; // Create a child thread

        if (pthread_create(&child_th, NULL, &DHCPOffer, &th_arg) != 0)
            error("There was an error creating a child process.");
    }

    close(socketfd);
    return 0;
}