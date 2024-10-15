import socket
from random import randint
import uuid

BROADCAST_IP = "255.255.255.255"
DHCP_PORT = 67


class DHCP_connection:
    def __init__(self):
        self.transaction_id = randint(0, 2**(32-1)).to_bytes(4, "big")
        self.hardware_id = uuid.getnode().to_bytes(16, "big")

    def DHCP_discover(self):
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

    def DHCP_Request(self, packet):
        # Suppose this thing is printing all the necessary info... (TODO!)
        # We say it's ok! please give me some of that IP params greatness!

        packet_mutable = bytearray(packet) # Create a mutable version of the packet

        packet_mutable[0] = (1).to_bytes(1, "big"); # Is a REQUEST type of operation
        
        # Decode the options received from server and replace the DHCP message type
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

    print("PYTHON CLIENT")
    dhcp = DHCP_connection()

    while True:
        packet_sent = dhcp.DHCP_discover()
        message = bytes(input("Press enter to send a DHCP Discover && Request message") + "\n", 'utf-8')
        mysock.sendto(packet_sent, (BROADCAST_IP, DHCP_PORT))
        print(f"The id for the transaction is: {int.from_bytes(dhcp.transaction_id, 'big')}")
        print(f"This pc's hardware id is: {int.from_bytes(dhcp.hardware_id, 'big')}")
        buffer, connection_address = mysock.recvfrom(576)
        print("Packet received from server: ")
        print(buffer)

        packet_sent = dhcp.DHCP_Request(buffer);
        mysock.sendto(packet_sent, (BROADCAST_IP, DHCP_PORT))
        buffer, connection_address = mysock.recvfrom(576)
        print("Packet received from server: ")
        print(buffer);