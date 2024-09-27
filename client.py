import socket

if __name__ == "__main__":
    mysock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    mysock.bind(("127.0.0.1", socket.INADDR_ANY))
    print("PYTHON CLIENT")
    while True:
        message = bytes(input("") + "\n", 'utf-8')
        mysock.sendto(message, ("127.0.0.1", 67))

        buffer, connection_address = mysock.recvfrom(255)
        message = buffer.decode('utf-8')
        print(message)