#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>

#define UDP_PACKET_SIZE 576
#define UDP_SERVER_PORT 67

struct client_data { // child process arguments
    int socketfd;
    struct sockaddr* client_addr;
    int client_len;
    char buffer[UDP_PACKET_SIZE];
};


/******************************* UTILITY FUNCTIONS ********************************/

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/*

Calculates the amount of bits a subnet mask is using by doing bitwise AND operations
of every octet of the mask

*/

int calc_subnet_bits(unsigned char* subnet_mask) {
    int total_bits = 0;
    unsigned char* subnet;
    unsigned char* token;

    strcpy(subnet, subnet_mask);
    token = strtok(subnet, "."); // Splits the subnet mask into the 4 octets

    // Loop while there are more tokens to get 
    while (token != NULL) {
        // Each 'token' is the octet which is separated with the delimiter (.)
        int current_oct = atoi(token);

        /*
            Comparisons are made by doing a bitwise AND operation with the number 1.
            if the LSB from the current octet is 1, it means is using that bit, so,
            the bitwise AND returns 1. Next, the octet is shifted 1 bit to the right
            to make the comparison with the next LSB.

            The process repeats until all bits are shifted and the value of the octet
            is 0.
        */
        while (current_oct != 0) {
            total_bits += current_oct & 1;
            current_oct = current_oct >> 1;
        }

        // Get the next octet
        token = strtok(NULL, ".");
    }

    return total_bits;
}

void get_available_address() {
    FILE* file = fopen("../addresses.txt", "r");
    unsigned char line[256];
    unsigned char subnet[256];
    unsigned char* token;
    int current_line = 1;

    fgets(line, sizeof(line), file);
    strcpy(subnet, line);
    fclose(file);

    token = strtok(subnet, "=");
    token = strtok(NULL, "=");

    printf("%i", calc_subnet_bits(token));
}

void serialize_int(unsigned char* buffer, uint64_t value, int offset, int bytes) {
    int current_offset = offset;
    for (int i = bytes; i > 0; i--) {
        buffer[current_offset] = value >> (8*(i-1)) & 0xFF;
        current_offset += 1;
    }
}

void serialize_char(unsigned char* buffer,int offset, char value) {
    buffer[offset] = value;
}

/* The number is created by doing a sum of every byte inside the buffer
    Every byte represents a value from 0 to 255 (that's why it's an
    unsigned char), and they are converted to an unsigned int of 64 bit
    (8 bytes). Since each byte it's just 8 bits, they are always put in the
    right-most side of the number in binary. Since we are dealing with Big Endian,
    the most significant bit it's always the first element in pos A and not A+size.
    So, we shift the current byte to its corresponding position, taking in count
    that it always starts at the first 8 bits or first byte of the 32 bit number
    So, if the number is 4 bytes long, buffer[i] << 8*(4-1) shifts its value to
    the most significant position in a 4 byte number (24 + 8), and so on.
*/
uint64_t get_nbyte_number(char* buffer, int offset, int bytes) {
    uint64_t number = 0; // accumulator for final number
    int current_offset = offset; // byte position in buffer where number starts

    // Loop until you checked all corresponding bytes
    for (int i = bytes; i > 0; i--) {
        number += (uint64_t)(unsigned char)buffer[current_offset] << (8*(i - 1));
        current_offset += 1;
    }
    return number;
}

uint32_t get_ip(struct sockaddr* ip_address) {
    return (*(struct sockaddr_in*)ip_address).sin_addr.s_addr;
}

void show_ip_repr(struct sockaddr* ip_address) {
    uint32_t ip_int = (*(struct sockaddr_in*)ip_address).sin_addr.s_addr;
    printf("%u\n", ip_int);

    // IPv4 takes 1 byte for every dot (.) separated number from 0 to 255; x.x.x.x 4 bytes
    unsigned char ip_bytes[4];
    
    ip_bytes[0] = ip_int;
    ip_bytes[1] = ip_int >> 8;
    ip_bytes[2] = ip_int >> 16;
    ip_bytes[3] = ip_int >> 24;

    printf("%d.%d.%d.%d\n", ip_bytes[0],ip_bytes[1],ip_bytes[2],ip_bytes[3]);
}

void print_packet(char* buffer, int size) {
    for (int i = 0; i < size; i++) {
        printf("%02x ", (u_int8_t)buffer[i]);
    }
    printf("\n");
}



/******************************* DHCP OPERATIONS ********************************/



void* DHCPOffer(void* args) {
    struct client_data client_args = *(struct client_data*)args;
    uint32_t offered_ip_dummy = 3232235819;

    serialize_int(client_args.buffer, (u_int64_t) 2, 0, 1);
    serialize_int(client_args.buffer, (u_int64_t) offered_ip_dummy, 20, 4);


    sendto(client_args.socketfd, client_args.buffer, sizeof(client_args.buffer), 0, client_args.client_addr, client_args.client_len);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t clientLength;

    get_available_address();
    exit(0);

    int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        error("Error Opening socket");
    
    bzero((char *) &server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_SERVER_PORT);

    if (bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        error("Error trying to bind the server");

    clientLength = sizeof(client_addr);

    while (1) {
        unsigned buffer[UDP_PACKET_SIZE]; // Initialize buffer for messages
        bzero(buffer, UDP_PACKET_SIZE); // Fill all the bytes in the buffer with zeros

        recvfrom(socketfd, buffer, UDP_PACKET_SIZE, 0, (struct sockaddr*) &client_addr, &clientLength);

        struct client_data th_arg; // Defines an struct for arguments to the child function
        th_arg.client_len = clientLength;
        th_arg.client_addr = (struct sockaddr*) &client_addr;
        th_arg.socketfd = socketfd;
        memcpy(th_arg.buffer, buffer, UDP_PACKET_SIZE);

        print_packet(th_arg.buffer, UDP_PACKET_SIZE);

        pthread_t child_th; // Store the created child thread

        if (pthread_create(&child_th, NULL, &DHCPOffer, &th_arg) != 0)
            error("There was an error creating a child process.");
    }

    close(socketfd);
    return 0;
}