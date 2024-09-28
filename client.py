import socket
from random import randint
import uuid

BROADCAST_IP = "255.255.255.255"
DHCP_PORT = 67


class DHCP_connection:
    def __init__(self):
        self.transaction_id = randint(0, 2**(32-1)).to_bytes(4, "big")

    def DHCP_discover(self):
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
        packet_sent = dhcp.DHCP_discover()
        message = bytes(input("Press enter to send a DHCP Discover message") + "\n", 'utf-8')
        mysock.sendto(packet_sent, (BROADCAST_IP, DHCP_PORT))
        print(f"The id for the transaction is: {int.from_bytes(dhcp.transaction_id, 'big')}")
        buffer, connection_address = mysock.recvfrom(576)
        message = buffer.decode('utf-8')
        print("the message is: " + message)