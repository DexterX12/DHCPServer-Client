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

class DHCP_connection:
    def DHCP_discover(self):
        packet = b''
        packet += (1).to_bytes(1, "big") # op code (1) BOOTRequest
        packet += (1).to_bytes(1, "big") # htype (1) ethernet by default
        packet += (6).to_bytes(1, "big") # hlen (6) 10mb ethernet by default
        packet += b'\x00' # hops (0) by default for client
        packet += randint(0, 2**(32)).to_bytes(4, "big") # xid - which is a random 4 byte transaction id
        packet += (0).to_bytes(2, "big") #secs - seconds elapsed while doing renewal or acquisition
        packet += (0b1000000000000000).to_bytes(2, "big") # leftmost bit determines if the server responds by BROADCAST or UNICAST
        packet += (0).to_bytes(4, "big") # ciaddr - client address, by default on 0.0.0.0
        packet += (0).to_bytes(4, "big") # yiaddr - "your" (client) address
        packet += (0).to_bytes(4, "big") # siaddr - IP address of the server
        packet += (0).to_bytes(4, "big") # giaddr - Relay agent IP address
        packet += (uuid.getnode().to_bytes(16, "big")) # chaddr - Client hardware address (e.g MAC)
        packet += (0).to_bytes(64, "big") # sname - server host name (null terminated)
        # packet += (0).to_bytes(128, "big") # file - optional boot file name
        # packet += (0).to_bytes(312, "big") # options - optional parameters field
        packet += b'\xff'
        return packet


if __name__ == "__main__":
    mysock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Set the client to send messages via broadcast when trying to get IP args
    mysock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Bind the current client local address to any available port
    mysock.bind(("0.0.0.0", socket.INADDR_ANY))

    print("PYTHON CLIENT")

    while True:
        dhcp = DHCP_connection()
        message = bytes(input("Press enter to send a DHCP Discover message") + "\n", 'utf-8')
        print(dhcp.DHCP_discover())
        mysock.sendto(dhcp.DHCP_discover(), (BROADCAST_IP, DHCP_PORT))

        buffer, connection_address = mysock.recvfrom(576)
        message = buffer.decode('utf-8')
        print(message)