# Printable-Chars-Counting-Server

## Description
This repository contains a client-server application designed to count printable characters in a stream of bytes sent from the client to the server. The server counts the number of printable characters (bytes with values between 32 and 126) and returns this count to the client. Additionally, the server maintains overall statistics on the number of printable characters received from all clients and prints these statistics upon termination.

## Usage
### Server
1. Compile the server code:
    ```sh
    gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 pcc_server.c -o pcc_server
    ```
2. Run the server:
    ```sh
    ./pcc_server <port>
    ```
    Replace `<port>` with the desired port number (a 16-bit unsigned integer).

### Client
1. Compile the client code:
    ```sh
    gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 pcc_client.c -o pcc_client
    ```
2. Run the client:
    ```sh
    ./pcc_client <server_ip> <port> <file_path>
    ```
    Replace `<server_ip>` with the server's IP address, `<port>` with the server's port number, and `<file_path>` with the path to the file to be sent.

## Output
The server will count the number of printable characters in the byte stream received from the client and send this count back to the client. The client will print the number of printable characters to standard output.

### Server Output
Upon receiving a SIGINT signal (e.g., Ctrl-C), the server will print the total counts of each printable character observed across all client connections to standard output in the following format, for each character:
```sh
char '%c' : %u times
```
### Client Output
The client will print the number of printable characters received from the server in the following format:
```sh
number of printable characters: %u
```

