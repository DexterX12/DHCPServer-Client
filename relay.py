import threading
import socket
import sys

dhcp_server_address = "" # User defined

def server_to_client(_socket, client_addr):
    buffer, server_address = _socket.recvfrom(576)
    _socket.sendto(buffer, (client_addr[0], client_addr[1]))

if __name__ == "__main__":
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", 67))

    if (len(sys.argv) < 2):
        raise Exception("You need to specify the DHCP Server's IP address.")

    dhcp_server_address = sys.argv[1]

    while True:
        buffer, client_address = sock.recvfrom(576)
        sock.sendto(buffer, (dhcp_server_address, 67))
        thread = threading.Thread(target=server_to_client, args=(sock, client_address, ), daemon=True)
        thread.start()




    
    
