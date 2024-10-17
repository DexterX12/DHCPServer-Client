import socket
from random import randint
import uuid
import struct

BROADCAST_IP = "158.247.127.78"
DHCP_PORT = 67

#Entero a IP - Usa socket y struct
def int_to_ip(integer):
    try:
        return socket.inet_ntoa(struct.pack('!I', integer))
    except:
        return "No IPv4"

class CLIENT_NETWORK_PARAMS:
    def __init__(self):
        self.__ip_address = 0
        self.__subnet_mask = 0
        self.__default_gateway = 0
        self.__dns_address = 0
        self.__lease_exp = 0
    
    def set_ip_address(self, ip):
        self.__ip_address = ip
    
    def set_subnet_mask(self, subnet):
        self.__subnet_mask = subnet
    
    def set_default_gateway(self, gateway):
        self.__default_gateway = gateway

    def set_dns_address(self, dns):
        self.__dns_address = dns

    def set_lease_expiration (self, lease_epoch):
        self.__lease_exp = lease_epoch

    def get_ip_address(self):
        return self.__ip_address
    
    def get_subnet_mask(self):
        return self.__subnet_mask
    
    def get_default_gateway(self):
        return self.__default_gateway
    
    def get_dns_address(self):
        return self.__dns_address
    
    def get_lease_expiration (self):
        return self.__lease_exp
    

class DHCP_CONNECTION:
    def __init__(self):
        self.transaction_id = randint(0, 2**(32-1)).to_bytes(4, "big")
        self.hardware_id = uuid.getnode().to_bytes(16, "big")

    def DHCPDISCOVER(self):
        options = ["IP_LEASE_TIME=", "DHCP_MESSAGE_TYPE=DHCPDISCOVER"]
        options_str = "\n".join(options)

        packet = b''
        packet += (1).to_bytes(1, "big") # op code (1) BOOTRequest
        packet += (1).to_bytes(1, "big") # htype (1) ethernet by default
        packet += (6).to_bytes(1, "big") # hlen (6) 10mb ethernet by default
        packet += b'\x00' # hops (0) by default for client
        packet += self.transaction_id # xid - which is a random 4 byte transaction id
        packet += (0).to_bytes(2, "big") #secs - seconds elapsed while doing renewal or acquisition
        packet += (0b1000000000000000).to_bytes(2, "big") # leftmost bit determines if the server responds by BROADCAST or UNICAST
        packet += (0).to_bytes(4, "big") # ciaddr - client address, by default on 0.0.0.0
        packet += (0).to_bytes(4, "big") # yiaddr - "your" (client) address
        packet += (0).to_bytes(4, "big") # siaddr - IP address of the server
        packet += (0).to_bytes(4, "big") # giaddr - Relay agent IP address
        packet += self.hardware_id # chaddr - Client hardware address (e.g MAC)
        packet += (0).to_bytes(64, "big") # sname - server host name (null terminated)
        packet += (0).to_bytes(128, "big") # file - optional boot file name

        # Create a NULL filled byte array with the minimum size of the options field (312)
        options_byte = bytearray([0 for i in range(0, 312)]) 
        # Put the default options for the DHCP message as bytes
        options_byte[:len(options_str)] = bytes(options_str, "ascii", "ignore")

        # Convert the bytearray to a bytestring to complete the packet
        packet += bytes(options_byte) # options - optional parameters field
        packet += b'\xff'

        return packet

    def DHCPREQUEST(self, packet):
        # Suppose this thing is printing all the necessary info... (TODO!)
        # We say it's ok! please give me some of that IP params greatness!

        packet_mutable = bytearray(packet) # Create a mutable version of the packet

        #packet_mutable[0] = (1).to_bytes(1, "big"); # Is a REQUEST type of operation
        
        # Decode the options received from server and replace the DHCP message type
        options = packet_mutable[236:].decode('ascii', "ignore").replace("DHCPOFFER", "DHCPREQUEST")

        # Set the replaced options as bytes inside the null-filled option bytearray
        packet_mutable[236:] = bytes(options, "ascii")
        
        # Return the packet as a byte string (inmutable)
        return bytes(packet_mutable)
    
    def renew_lease(self, client_ip):
        # Create a "base" DHCP packet with the DISCOVER message
        packet_mutable = bytearray(self.DHCPDISCOVER())

        # Put the client's ip inside the packet
        packet_mutable[16:20] = bytes(client_ip, "ascii", "ignore")

        # Decode the options received from server and replace the DHCP message type
        # RENEWING state is specified as a DHCPREQUEST message by the RFC2131
        options = packet_mutable[236:].decode('ascii', "ignore").replace("DHCPOFFER", "DHCPREQUEST")

        # Create a NULL filled byte array with the minimum size of the options field (312)
        options_byte = bytearray([0 for i in range(0, 312)])

        # Set the replaced options as bytes inside the null-filled option bytearray
        options_byte[:len(options)] = bytes(options, "ascii", "ignore")

        # Modify the original (mutable) packet with the new options
        packet_mutable[236:] = options_byte
        
        # Return the packet as a byte string (inmutable)
        return bytes(packet_mutable)



if __name__ == "__main__":
    mysock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Set the client to send messages via broadcast when trying to get IP args
    mysock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Bind the current client local address to any available port
    mysock.bind(("0.0.0.0", socket.INADDR_ANY))
    mysock.settimeout(3)

    print("PYTHON CLIENT")
    dhcp = DHCP_CONNECTION()

    while True:
        packet_sent = dhcp.DHCPDISCOVER()
        message = bytes(input("Press enter to send a DHCP Discover && Request message") + "\n", 'utf-8')
        mysock.sendto(packet_sent, (BROADCAST_IP, DHCP_PORT))
        print(f"The id for the transaction is: {int.from_bytes(dhcp.transaction_id, 'big')}")
        print(f"This pc's hardware id is: {int.from_bytes(dhcp.hardware_id, 'big')}")
        try:
            buffer, connection_address = mysock.recvfrom(576)
        except TimeoutError:
            print("Failed trying to reach DHCP Server... Trying again")
            break
        
        print("Successfully connected to DHCP Server...")

        packet_sent = dhcp.DHCPREQUEST(buffer);
        mysock.sendto(packet_sent, (BROADCAST_IP, DHCP_PORT))
        buffer, connection_address = mysock.recvfrom(576)

        print("DHCP Server has sent the following parameters: ")
        print(f'IP-ADRESS = {int_to_ip(int.from_bytes(buffer[16:20], "big"))}')

        #Terminar
        for line in buffer[236:].decode("ascii","ignore").split('\n'):
            if line.startswith("IP_LEASE_TIME") or line.startswith("DHCP_MESSAGE_TYPE"):
                print(f"{line}") #Imprimir la línea tal cual
            else:
                key, value = line.split('=') #Separar en nombre y valor para convertir de int a ip
                print(f"{key} = {int_to_ip(int(value.strip("\x00")))}")