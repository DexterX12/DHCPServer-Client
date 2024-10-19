# DHCP Protocol Implementation

## Introduction

This project is a simple implementation of the DHCP Protocol, which is a protocol used to dinamically assign network parameters to clients in a network.

It was made on C for the server side, and Python on the client side and for the relay device.

## Development and design

Both server and client were developed having in mind the next two cases:

**Server & client inside the same network**

![image](https://github.com/user-attachments/assets/bb6b330b-b8a2-4d87-a80f-6c5092120eb2)

Here, both the server and client are in the same network, so the client is able to do a DHCPDISCOVER message and the server will receive it with no issues.

The server was designed to identify the main DHCP message type from the client: DHCPDISCOVER, DHCPREQUEST and DHCPRELEASE, and it is independent of the network that the clients are in.

**Server & client in separated subnets**

![image](https://github.com/user-attachments/assets/9aa831ab-00f5-427d-bad7-43ea9ac3bf6b)

In this case, the server is located in a different subnet than the client. Naturally, when the client does a DHCPDISCOVER, it will not reach the server and fail.

The DHCP Relay functionality was implemented to allow the client to reach a server that is within a single hop, so, the DHCPDISCOVER message can finally reach the server and viceversa.

**Transport layer protocol used**

The protocol UDP was chosen for this project.

Mainly because it's the original protocol in which the DHCP protocol works, and also because it's fast for retrieving packets between hosts. Since the amount of hops made for this use case are fairly low, the risks of losing packets are too, so it's ideal for this kind of implementation.

**DHCP Server network configuration**

The user is able to modify the network's hosts capacity and the network's identifier by modifying a file called _addresses.txt_.

Having the user to choose the network's IP range is quite handful, but at the same time it needs to match the subnet mask given to the client. So, instead of manually assigning the range by users' choice, they can manually change the CIDR inside the configuration file. The server automatically calculates the subnet mask and available addresses to host, excluding the network's identifier, BROADCAST address and gateway address (which can be edited too inside the file).

Users are able to modify too the lease time inside the same configuration file. The lease time is measured in seconds.

## Achieved and not achieved aspects

**Achieved aspects**

- The server is able to handle main DHCP messages (DORA)
- The server is able to assign a lease for each connected client
- The server is able to handle DHCPRELEASE requests
- The server network is configured by a user-stablished configuration file
- The client is able to renew its lease
- The client is able to release its lease upon leaving
- The client shows the given network parameters from the server
  
**Not achieved aspects**
  - The server does not use the subnet obtained from the computer's real network, so it does not assign unique IPs depending on the client's subnet

# How to run

Clone this repository and follow these steps:

It's important to have a C/C++ compiler such as `gcc` and the Python interpreter.

To run the server you need to be inside a UNIX system, since it uses the pthread library for threading and it's not compatible with Windows. Run the server by using the following command: 

`gcc -pthread -o server server.c && sudo ./server`

Privileged execution is needed because of the use of a reserved port (67).

To run the client, use the following command:

`python client.py`

To run the DHCP Relay agent, run the following commmand:

`sudo python relay.py <SERVER_IP_ADDRESS>`

Here, you need to specify the DHCP Server that the relay agent will reach to. Make sure that the relay agent can see both subnets (Server and client subnets). This also needs a privileged execution because of the use of the reserved port 67.

# Final thoughts and conclusion

By analizing and studying how the DHCP Protocol works, its usefulness within networks (and even SOFO ones) is definitely apparent; this implementation allowed us to deeply understand how devices communicate between them inside a network, the manipulation of data inside packets, byte order, endianness and more.

# References
https://datatracker.ietf.org/doc/html/rfc2131

https://docs.python.org/3/library/socket.html#socket-objects

https://learn.microsoft.com/es-es/windows-server/troubleshoot/dynamic-host-configuration-protocol-basics

https://linuxhowtos.org/C_C++/socket.htm

https://www.omnicalculator.com/other/ip-subnet#:~:text=How%20do%20I%20calculate%20the,number%20of%20usable%20IP%20addresses
