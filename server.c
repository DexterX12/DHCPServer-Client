#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>
#include <time.h> 

#define UDP_PACKET_SIZE 576
#define OPTIONS_BYTE_SIZE 312
#define UDP_SERVER_PORT 67

typedef struct { // child process arguments
    int socketfd;
    struct sockaddr* client_addr;
    int client_len;
    struct sockaddr_in server_addr;
    char buffer[UDP_PACKET_SIZE];
} socket_data;

typedef struct {
    uint32_t IP_ADDR; // Client's assigned IP address
    uint64_t MAC_ADDR; // Client's MAC address
    time_t LEASE; // client's assigned lease time
    int AVAILABLE; // 0 for already in use / offered but not accepted (yet), 1 for available
} addr_lease;

typedef struct {
    uint32_t NETWORK_ADDR; // Network's identifier
    uint32_t SUBNET_MASK; // Network's subnet mask
    uint32_t DNS; // Network's DNS
    uint32_t DEFAULT_GATEWAY;
    int LEASE_TIME; // Network's default lease time
    addr_lease* ADDR_LIST; // Its size will be defined by the subnet mask
    int ADDR_LIST_SIZE; // Self explanatory
} addr_pool;

// This type definition exists for the creation of an array of DHCP Operations
// This can let easily modify the amount of messages for reply to the client
// And be order-independent
typedef void* (*DHCP_FUNCTIONS)(void*);

const char* DHCP_MESSAGES[] = {"DHCPDISCOVER", "DHCPREQUEST"};
addr_pool address_pool;


/******************************* UTILITY FUNCTIONS ********************************/

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

char* int_to_str(int value) {
    char* buffer;
    int length = snprintf(NULL, 0, "%d", value);
    buffer = malloc(sizeof(char) * (length+1));
    snprintf(buffer, sizeof(buffer), "%d", value);

    return buffer;
}

char* int_to_mac(uint64_t mac) {
    int size = snprintf(NULL, 0, "%lx", mac);
    char hex[size];
    sprintf(hex, "%lx", mac);
    
    char* hex_sep = malloc((sizeof(char) * (size + (size/2))));
    int pos_counter = 1;
    
    for (int i = 0; i < sizeof(hex)/sizeof(hex[0]); i++) {
        char char_str[] = {hex[i]};
        
        strcat(hex_sep, char_str);
        
        if ((i + 1)%2 == 0 && (i + 1) < sizeof(hex)/sizeof(hex[0]) - 1)
            strcat(hex_sep, ":");
    }
    
    return hex_sep;
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

    unsigned char* str_base_ip = malloc(sizeof(ip));
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
    unsigned char* ip_str = malloc(sizeof(token2));
    strcpy(ip_str, token2);

    token2 = strtok(NULL, "/");
    unsigned char* CIDR = malloc(sizeof(token2));
    strcpy(CIDR, token2);

    available_addresses = (1 << (32 - atoi(CIDR))) - 2; // 2 for network's address & broadcast
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

    // Assigning all IPs as available, with the exception of the gateway IP
    for (int i = 0; i < available_addresses; i++) {
        if ((network_address + (i+1)) == network_gateway)
            pool->ADDR_LIST[i].AVAILABLE = 0;    
        else  
            pool->ADDR_LIST[i].AVAILABLE = 1;

        pool->ADDR_LIST[i].IP_ADDR = network_address + (i+1);
    }

    pool->ADDR_LIST_SIZE = available_addresses;

    free(ip_str);
    free(CIDR);
}

time_t get_lease(int base_lease) {
    return time(NULL) + base_lease;
}

int get_dhcp_message(unsigned char* buffer) {
    unsigned char options[OPTIONS_BYTE_SIZE]; // Initialize buffer for all possible options
    char* token;

    for (int i = 0; i < OPTIONS_BYTE_SIZE; i++) {
        options[i] = buffer[236 + i]; // 236 is the beginning of the options field inside the packet
    }

    token = strtok(options, "\n");
    token = strtok(NULL, "\n");

    char* token2;
    token2 = strtok(token, "=");
    token2 = strtok(NULL, "=");

    for (int i = 0; i < (sizeof(DHCP_MESSAGES) / sizeof(DHCP_MESSAGES[0])); i++) {
        if (strcmp(token2, DHCP_MESSAGES[i]) == 0) {
            return i;
        }
    }

    return -1;
}

void serialize_int(unsigned char* buffer, uint64_t value, int offset, int bytes) {
    int current_offset = offset;
    for (int i = bytes; i > 0; i--) {
        buffer[current_offset] = value >> (8*(i-1)) & 0xFF;
        current_offset += 1;
    }
}

void serialize_char(unsigned char* buffer, char* value, int offset) {
    for (int i = 0; i < strlen(value); i++) {
        buffer[offset + i] = value[i];
    }
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
    
    printf("\n%s\n", "Current parameters for this DHCP server:");
    printf("%s", "Network address: ");
    printf("%d.%d.%d.%d\n", network_ip_str[0], network_ip_str[1], network_ip_str[2], network_ip_str[3]);
    printf("%s", "Network subnet mask: ");
    printf("%d.%d.%d.%d\n", network_subnet_str[0], network_subnet_str[1], network_subnet_str[2], network_subnet_str[3]);
    printf("%s", "Network default gateway: ");
    printf("%d.%d.%d.%d\n", network_gateway_str[0], network_gateway_str[1], network_gateway_str[2], network_gateway_str[3]);
    printf("%s", "Network DNS: ");
    printf("%d.%d.%d.%d\n", network_dns_str[0], network_dns_str[1], network_dns_str[2], network_dns_str[3]);
    printf("%s", "Available addresses: ");
    printf("%i | (%i without gateway)\n", (pool->ADDR_LIST_SIZE) - 1, pool->ADDR_LIST_SIZE);
    printf("%s", "Current lease time: ");
    printf("%i seconds\n", pool->LEASE_TIME);

    free(network_ip_str);
    free(network_dns_str);
    free(network_gateway_str);
    free(network_subnet_str);
}

void show_lease (addr_pool* pool) {
    printf("%s\n", "    MAC ADDRESS    |    IP ADDRESS    |    LEASE EXPIRATION (EPOCH)");
    printf("%s\n", "-----------------------------------------------------------------------");
    
    for (int i = 0; i < (*pool).ADDR_LIST_SIZE; i++) {
        unsigned char* mac_str = int_to_mac((*pool).ADDR_LIST[i].MAC_ADDR);
        unsigned char* ip_str = int_to_ip((*pool).ADDR_LIST[i].IP_ADDR);
        time_t lease_epoch = (*pool).ADDR_LIST[i].LEASE;

        if ((*pool).ADDR_LIST[i].IP_ADDR == (*pool).DEFAULT_GATEWAY)
            continue;
        

        if ((*pool).ADDR_LIST[i].AVAILABLE)
            continue;

        printf("%s      ", mac_str);
        printf("%d.%d.%d.%d         ", ip_str[0],ip_str[1],ip_str[2],ip_str[3]);
        printf("%lu\n", lease_epoch);
    }
}

/******************************* DHCP OPERATIONS ********************************/

void* DHCPOFFER(void* args) {
    socket_data client_args = *(socket_data*)args;
    uint32_t yiaddr;
    int yiaddr_pos; // IP offered to client's position, if the operation fails or declines
    char opt_lease[64] = "IP_LEASE_TIME=";
    char* opt_message_type = "DHCP_MESSAGE_TYPE=DHCPOFFER";
    unsigned char* lease_time_str = int_to_str(address_pool.LEASE_TIME);
    
    strcat(opt_lease, lease_time_str);
    strcat(opt_lease, "\n");

    free(lease_time_str);

    for (int i = 0; i < address_pool.ADDR_LIST_SIZE; i++) {

        if (address_pool.ADDR_LIST[i].IP_ADDR == address_pool.DEFAULT_GATEWAY)
            continue;

        if (address_pool.ADDR_LIST[i].AVAILABLE || (address_pool.ADDR_LIST[i].AVAILABLE && time(NULL) >= address_pool.ADDR_LIST[i].LEASE)) {
            yiaddr = address_pool.ADDR_LIST[i].IP_ADDR;
            address_pool.ADDR_LIST[i].AVAILABLE = 0; // IP reservation for the operation
            yiaddr_pos = i;
            break;
        }
    }

    serialize_int(client_args.buffer, 2, 0, 1);
    serialize_int(client_args.buffer, yiaddr, 16, 4);
    serialize_int(client_args.buffer, client_args.server_addr.sin_addr.s_addr, 20, 4);
    serialize_char(client_args.buffer, opt_lease, 236);
    serialize_char(client_args.buffer, opt_message_type, 236 + strlen(opt_lease));

    sendto(client_args.socketfd, client_args.buffer, sizeof(client_args.buffer), 0, client_args.client_addr, client_args.client_len);
}

void* DHCPACK (void* args) {
    socket_data client_args = *(socket_data*)args;
    uint32_t offered_ip;
    char opt_lease[64] = "IP_LEASE_TIME=";
    char* opt_message_type = "DHCP_MESSAGE_TYPE=DHCPACK";
    unsigned char* lease_time_str = int_to_str(address_pool.LEASE_TIME);
    
    strcat(opt_lease, lease_time_str);
    strcat(opt_lease, "\n");

    free(lease_time_str);

    offered_ip = (uint32_t) get_nbyte_number(client_args.buffer, 16, 4);
    for (int i = 0; i < address_pool.ADDR_LIST_SIZE; i++) {
        if (address_pool.ADDR_LIST[i].IP_ADDR == offered_ip) {
            address_pool.ADDR_LIST[i].MAC_ADDR = get_nbyte_number(client_args.buffer, 28, 16);
            address_pool.ADDR_LIST[i].LEASE = get_lease(address_pool.LEASE_TIME);
        }
    }

    serialize_int(client_args.buffer, 0, 232, UDP_PACKET_SIZE - 232);
    serialize_char(client_args.buffer, opt_lease, 232);
    serialize_char(client_args.buffer, opt_message_type, 232 + strlen(opt_lease));

    sendto(client_args.socketfd, client_args.buffer, sizeof(client_args.buffer), 0, client_args.client_addr, client_args.client_len);
}

void* DHCPNAK (void* args) {
    socket_data client_args = *(socket_data*)args;
    char* opt_message_type = "DHCP_MESSAGE_TYPE=DHCPACK";
}

/**************************** MENU ************************************/

void* server_initialize() {
    struct sockaddr_in server_addr, client_addr;
    const DHCP_FUNCTIONS dhcp_func_table[] = {&DHCPOFFER, &DHCPACK};
    socklen_t clientLength;

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

    set_network_params(&address_pool);
    show_network_params(&address_pool);

    pthread_t menu_thread;

    /*
        Connection loop. All requests will be managed in this loop.
        Every DHCP message will be managed on a seperate thread,
        allowing to use this thread for serving any request.
    */

    while (1) {
        unsigned char buffer[UDP_PACKET_SIZE]; // Initialize buffer for messages

        bzero(buffer, UDP_PACKET_SIZE); // Fill all the bytes in the buffer with zeros

        recvfrom(socketfd, buffer, UDP_PACKET_SIZE, 0, (struct sockaddr*) &client_addr, &clientLength);

        socket_data th_arg; // Defines an struct for arguments to the child function
        th_arg.client_len = clientLength;
        th_arg.client_addr = (struct sockaddr*) &client_addr;
        th_arg.server_addr = server_addr;
        th_arg.socketfd = socketfd;
        memcpy(th_arg.buffer, buffer, UDP_PACKET_SIZE);

        pthread_t child_th; // Store the created child thread

        if (get_dhcp_message(buffer) == -1) {
            printf("Non valid DHCP message received.");
            continue;
        }

        if (pthread_create(&child_th, NULL, dhcp_func_table[get_dhcp_message(buffer)], &th_arg) != 0)
            error("There was an error creating a child process.");

    }

    close(socketfd);
}


int main() {

    pthread_t dhcp_listen;


    if (pthread_create(&dhcp_listen, NULL, &server_initialize, NULL) != 0)
        error("There was an error creating a child process.");

    while(1) {
        printf("\n\n%s\n", "Press enter to check current leases");
        getchar();
        show_lease(&address_pool);
    }


    return 0;
}