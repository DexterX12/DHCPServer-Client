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

typedef struct { // child process arguments
    int socketfd;
    struct sockaddr* client_addr;
    int client_len;
    char buffer[UDP_PACKET_SIZE];
} client_data;

typedef struct {
    uint32_t IP_ADDR; // Client's assigned IP address
    uint64_t MAC_ADDR; // Client's MAC address
    int LEASE; // client's assigned lease time
} addr_lease;

typedef struct {
    uint32_t NETWORK_ADDR; // Network's identifier
    uint32_t SUBNET_MASK; // Network's subnet mask
    uint32_t DNS; // Network's DNS
    uint32_t DEFAULT_GATEWAY;
    int LEASE_TIME; // Network's default lease time
    addr_lease* ADDR_LIST; // Its size will be defined by the subnet mask
} addr_pool;

addr_pool address_pool;

/******************************* UTILITY FUNCTIONS ********************************/

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

unsigned char* int_to_ip(uint32_t ip) {
    unsigned char* ip_bytes = malloc((sizeof(unsigned char) * 4));
    
    ip_bytes[0] = ip >> 24;
    ip_bytes[1] = ip >> 16;
    ip_bytes[2] = ip >> 8;
    ip_bytes[3] = ip;

    return ip_bytes;
}

uint32_t ip_to_int (unsigned char* ip) {
    uint32_t ip_int = 0;

    unsigned char* str_base_ip = (char*)malloc(sizeof(ip));
    char* token;
    int i = 4;

    strcpy(str_base_ip, ip);
    token = strtok(str_base_ip, ".");

    while (token != 0) {
        ip_int += (uint32_t) atoi(token) << 8*(i-1);
        token = strtok(NULL, ".");
        i -= 1;
    }

    return ip_int;
}

void set_network_params(addr_pool* pool) {
    FILE* file = fopen("../addresses.txt", "r");

    unsigned char line[256];
    unsigned char* token;

    uint32_t network_mask;
    uint32_t network_address;
    uint32_t network_gateway;
    uint32_t network_dns;
    int network_lease;
    int available_addresses;

    // Getting the network ip
    fgets(line, sizeof(line), file);
    token = strtok(line, "=");
    token = strtok(NULL, "=");
    unsigned char* token2 = strtok(token, "/");
    unsigned char* ip_str = (char*)malloc(sizeof(token2));
    strcpy(ip_str, token2);

    token2 = strtok(NULL, "/");
    unsigned char* CIDR = (char*)malloc(sizeof(token2));
    strcpy(CIDR, token2);

    available_addresses = (1 << (32 - atoi(CIDR))) - 2;
    network_mask = -1 << 32 - atoi(CIDR);
    network_address = ip_to_int(ip_str);
    network_address = network_mask & network_address;

    // Getting network's default gateway
    fgets(line, sizeof(line), file);
    token = strtok(line, "=");
    token = strtok(NULL, "=");

    network_gateway = ip_to_int(token);

    // Getting network's DNS
    fgets(line, sizeof(line), file);
    token = strtok(line, "=");
    token = strtok(NULL, "=");

    network_dns = ip_to_int(token);

    // Getting network's default lease time
    fgets(line, sizeof(line), file);
    token = strtok(line, "=");
    token = strtok(NULL, "=");

    network_lease = atoi(token);

    fclose(file);

    pool->NETWORK_ADDR = network_address;
    pool->SUBNET_MASK = network_mask;
    pool->DEFAULT_GATEWAY = network_gateway;
    pool->DNS = network_dns;
    pool->LEASE_TIME = network_lease;
    pool->ADDR_LIST = malloc(sizeof(addr_lease) * available_addresses);
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

void print_packet(char* buffer, int size) {
    for (int i = 0; i < size; i++) {
        printf("%02x ", (u_int8_t)buffer[i]);
    }
    printf("\n");
}

void show_network_params(addr_pool* pool) {
    unsigned char* network_ip_str = int_to_ip(pool->NETWORK_ADDR);
    unsigned char* network_dns_str = int_to_ip(pool->DNS);
    unsigned char* network_gateway_str = int_to_ip(pool->DEFAULT_GATEWAY);
    unsigned char* network_subnet_str = int_to_ip(pool->SUBNET_MASK);
    
    printf("%s", "Current parameters for this DHCP server:\n");
    printf("%s", "Network address: ");
    printf("%d.%d.%d.%d\n", network_ip_str[0], network_ip_str[1], network_ip_str[2], network_ip_str[3]);
    printf("%s", "Network subnet mask: ");
    printf("%d.%d.%d.%d\n", network_subnet_str[0], network_subnet_str[1], network_subnet_str[2], network_subnet_str[3]);
    printf("%s", "Network default gateway: ");
    printf("%d.%d.%d.%d\n", network_gateway_str[0], network_gateway_str[1], network_gateway_str[2], network_gateway_str[3]);
    printf("%s", "Network DNS: ");
    printf("%d.%d.%d.%d\n", network_dns_str[0], network_dns_str[1], network_dns_str[2], network_dns_str[3]);
}

/******************************* DHCP OPERATIONS ********************************/

void* DHCPOffer(void* args) {
    client_data client_args = *(client_data*)args;
    uint32_t offered_ip_dummy = 3232235806;

    serialize_int(client_args.buffer, (u_int64_t) 2, 0, 1);
    serialize_int(client_args.buffer, (u_int64_t) offered_ip_dummy, 20, 4);


    sendto(client_args.socketfd, client_args.buffer, sizeof(client_args.buffer), 0, client_args.client_addr, client_args.client_len);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t clientLength;

    // set_network_params(&address_pool);
    // show_network_params(&address_pool);
    // exit(0);

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

        client_data th_arg; // Defines an struct for arguments to the child function
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