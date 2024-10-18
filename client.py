import socket
from random import randint
import uuid
import struct
from time import time
import threading

BROADCAST_IP = "255.255.255.255"
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
        packet_mutable[16:20] = int.to_bytes(client_ip, 4, "big")

        # Decode the options received from server and replace the DHCP message type
        # RENEWING state is specified as a DHCPREQUEST message by the RFC2131
        options = packet_mutable[236:].decode('ascii', "ignore").replace("DHCPDISCOVER", "DHCPREQUEST")

        # Create a NULL filled byte array with the minimum size of the options field (312)
        options_byte = bytearray([0 for i in range(0, 312)])

        # Set the replaced options as bytes inside the null-filled option bytearray
        options_byte[:len(options)] = bytes(options, "ascii", "ignore")

        # Modify the original (mutable) packet with the new options
        packet_mutable[236:] = options_byte
        
        # Return the packet as a byte string (inmutable)
        return bytes(packet_mutable)
    
    def release_lease(self, client_ip):
        # Create a "base" DHCP packet with the DISCOVER message
        packet_mutable = bytearray(self.DHCPDISCOVER())

        # Put the client's ip inside the packet
        packet_mutable[12:16] = int.to_bytes(client_ip, 4, "big")
        packet_mutable[28:44] = self.hardware_id

        # Decode the options received from server and replace the DHCP message type
        # RENEWING state is specified as a DHCPREQUEST message by the RFC2131
        options = packet_mutable[236:].decode('ascii', "ignore").replace("DHCPDISCOVER", "DHCPRELEASE")

        # Create a NULL filled byte array with the minimum size of the options field (312)
        options_byte = bytearray([0 for i in range(0, 312)])

        # Set the replaced options as bytes inside the null-filled option bytearray
        options_byte[:len(options)] = bytes(options, "ascii", "ignore")

        # Modify the original (mutable) packet with the new options
        packet_mutable[236:] = options_byte
        
        # Return the packet as a byte string (inmutable)
        return bytes(packet_mutable)

def ip_lease(buffer):
    buffer_data = buffer.decode()
    lines = buffer_data.splitlines()
    ip_lease_time=None

    for line in lines:
        if line.startswith("IP_LEASE_TIME="):
            ip_lease_time = line.split("=")[1]
            break

    total_lease = int(ip_lease_time) + time()
    
    return total_lease, ip_lease_time

def buffer_lines(buffer_ip, buffer):
    buffer_data_ip = int.from_bytes(buffer_ip, "big")
    param_dict = dict()
    param_dict["IP-ADDRESS"] = buffer_data_ip
    buffer_data = buffer.decode()
    lines = buffer_data.splitlines()
    for line in lines:
        if line.startswith("SUBNET_MASK"):
            param_dict["SUBNET_MASK"] = line.split("=")[1]
        elif line.startswith("DEFAULT_GATEWAY"):
            param_dict["DEFAULT_GATEWAY"] = line.split("=")[1]
        elif line.startswith("DNS_SERVER"):
            param_dict["DNS_SERVER"] = line.split("=")[1].strip("\x00")

    return param_dict

def renewal_loop(buffer, dhcp, client, sock):

    while(True):
        server_lease = int(ip_lease(buffer[236:])[1])
        if ((client.get_lease_expiration() - time()) <= (server_lease/2)):
            packet = dhcp.renew_lease(client.get_ip_address())
            sock.sendto(packet, (BROADCAST_IP, DHCP_PORT))
            buffer, connection_address = mysock.recvfrom(576)
            options = buffer[236:].decode()
            message = options.splitlines()[1].split("=")[1]
            if (message == "DHCPACK"):
                client.set_lease_expiration(client.get_lease_expiration() + server_lease)
                print(f"IP Lease time has been renewed! New expire time: {int(client.get_lease_expiration())}")



if __name__ == "__main__":
    mysock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Set the client to send messages via broadcast when trying to get IP args
    mysock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Bind the current client local address to any available port
    mysock.bind(("0.0.0.0", socket.INADDR_ANY))
    mysock.settimeout(8)

    print("PYTHON CLIENT")
    dhcp = DHCP_CONNECTION()
    cliente = CLIENT_NETWORK_PARAMS()
    buffer = None

    packet_sent = dhcp.DHCPDISCOVER()
    message = bytes(input("Press enter to send a DHCP Discover && Request message") + "\n", 'utf-8')
    mysock.sendto(packet_sent, (BROADCAST_IP, DHCP_PORT))
    print(f"The id for the transaction is: {int.from_bytes(dhcp.transaction_id, 'big')}")
    print(f"This pc's hardware id is: {int.from_bytes(dhcp.hardware_id, 'big')}")
    try:
        buffer, connection_address = mysock.recvfrom(576)
    except TimeoutError:
        print("Failed trying to reach DHCP Server...")
        exit(1)
    
    print("Successfully connected to DHCP Server...")

    print(int.from_bytes(buffer[20:24], "big"))
    packet_sent = dhcp.DHCPREQUEST(buffer);
    mysock.sendto(packet_sent, (BROADCAST_IP, DHCP_PORT))
    buffer, connection_address = mysock.recvfrom(576)

    print("DHCP Server has sent the following parameters: ")
    print(f'IP-ADDRESS = {int_to_ip(int.from_bytes(buffer[16:20], "big"))}')

    for line in buffer[236:].decode("ascii","ignore").split('\n'):
        if line.startswith("IP_LEASE_TIME") or line.startswith("DHCP_MESSAGE_TYPE"):
            pass
        else:
            key, value = line.split('=') #Separar en nombre y valor para convertir de int a ip
            value_formated = value.strip('\x00')
            print(f"{key} = {int_to_ip(int(value_formated))}")

    dicti = buffer_lines(buffer[16:20], buffer[236:])
    total_time = ip_lease(buffer[236:])[0]
    
    cliente.set_ip_address(int(dicti["IP-ADDRESS"]))
    cliente.set_default_gateway(int(dicti["DEFAULT_GATEWAY"]))
    cliente.set_dns_address(int(dicti["DNS_SERVER"]))
    cliente.set_subnet_mask(int(dicti["SUBNET_MASK"]))
    cliente.set_lease_expiration(total_time)

    menu = threading.Thread(target=renewal_loop, args=(buffer, dhcp, cliente, mysock, ), daemon=True)
    menu.start()

    input("Press enter to release your IP and exit the program")
    packet = dhcp.release_lease(cliente.get_ip_address())
    mysock.sendto(packet, (BROADCAST_IP, DHCP_PORT))
    exit(0)


    