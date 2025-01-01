#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>

#define PCCSize 95 // 126 - 32 + 1

// Macro to handle errors
#define handleError(msg) \
    perror(msg);         \
    exit(EXIT_FAILURE);

atomic_int handlingClient; // =1 if we are handling a client, =0 otherwise
atomic_int receivedSIGINT; // =1 if we received SIGINT signale, =0 otherwise

/*
    Data structure to hold the number of times each printable character was observed
    value 32 <= val <= 126 will be located in index 0 <= i <= 95
    --> i = val - 32
*/
unsigned int pccTable[PCCSize];

/*
    @brief Read data of given size from file descriptor to buffer
    @param fd File descriptor to read from
    @param buf Buffer to read data to
    @param size Size of data to read
    @return number of bytes read or -1 if connection terminated
*/
ssize_t readFromFD(int fd, void *buf, ssize_t size)
{
    memset(buf, 0, size);
    ssize_t totalRead = 0;    // How much already read
    ssize_t need2Read = size; // How much need to read
    ssize_t numRead = 1;      // How much read at the moment

    // Reading data while there is still data to read
    while (need2Read > 0 && numRead > 0)
    {
        numRead = read(fd, (char *)buf + totalRead, need2Read);
        totalRead += numRead;
        need2Read -= numRead;
    }

    // Handling errors
    if (numRead <= 0)
    {
        if (numRead == 0 || errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE)
        {
            perror("Client to server connection terminated");
            return -1; // Ignore current client and wait for next one
        }

        handleError("'read' failure");
    }

    return totalRead;
}

/*
    @brief Write data of given size from buffer to file descriptor
    @param fd File descriptor to write to
    @param buf Buffer to write data from
    @param size Size of data to write
    @return number of bytes written or -1 if connection terminated
*/
ssize_t write2FD(int fd, void *buf, ssize_t size)
{
    size_t totalSent = 0;     // How much already sent
    ssize_t need2Send = size; // How much need to send
    ssize_t numSent = 1;      // How much sent at the moment

    // Writing data while there is still data to write
    while (need2Send > 0 && numSent > 0)
    {
        numSent = write(fd, (char *)buf + totalSent, need2Send);
        totalSent += numSent;
        need2Send -= numSent;
    }

    // Handling errors
    if (numSent <= 0)
    {
        if (numSent == 0 || errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE)
        {
            perror("Client to server connection terminated");
            return -1; // Ignore current client and wait for next one
        }

        handleError("'write' failure");
    }

    return totalSent;
}

/*
    @brief Calculate the PCC of given buffer
    @param buf Buffer to calculate PCC on
    @param size Size of buffer
    @return PCC
*/
uint32_t calcPCC(char *buf, uint32_t size)
{
    int i;
    uint32_t pcc = 0;

    for (i = 0; i < size; i++)
    {
        if (buf[i] >= 32 && buf[i] <= 126)
        {
            pcc++;                   // Increasing total pcc of the buffer
            pccTable[buf[i] - 32]++; // Increasing the times the specific printable character was observed
        }
    }

    return pcc;
}

/*
    @brief Print the number of times each printable character was observed
    @return 0 on success
*/
int printTotalPCC()
{
    int i;
    
    for (i = 0; i < PCCSize; i++)
        printf("char '%c' : %u times\n", (char)(i + 32), pccTable[i]);

    return EXIT_SUCCESS;
}

/*
    @brief Handle SIGINT signal
    @param sigNum The signal number
*/
void handleSIGINT(int sigNum)
{
    // We aren't handling client so need to print and exit
    if (handlingClient == 0)
    {
        printTotalPCC(); // Printing the times each printable character was observed
        exit(EXIT_SUCCESS);
    }

    // We are handling client so need to finish handling it
    receivedSIGINT = 1;
}

/*
    @brief Initialize the environment
    @return 0 on success
*/
int initialize()
{
    handlingClient = 0;
    receivedSIGINT = 0;

    memset(pccTable, 0, sizeof(uint32_t) * PCCSize);

    // Setting handling signal behavior
    struct sigaction sa;
    sa.sa_handler = &handleSIGINT;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        handleError("'sigaction' failure");
    }

    return EXIT_SUCCESS;
}

/*
    @brief Main function
    @param argc Number of command line arguments
    @param argv Command line arguments
    @return 0 on success
    @pre argv[1] is the server's port
*/
int main(int argc, char *argv[])
{
    // Invalid number of arguments
    if (argc != 2)
    {
        handleError("Invalid number of arguments");
    }

    initialize();

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));

    // Opening socket
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        handleError("'socket' failure");
    }

    // Setting socket options
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        handleError("'setsockopt' failure");
    }

    // Setting the IPv4, port and address of the server
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(atoi(argv[1]));

    // Binding the server
    if (0 != bind(socketFD, (struct sockaddr *)&serverAddr, sizeof(serverAddr)))
    {
        handleError("'bind' failure");
    }

    // Making the server listen
    if (listen(socketFD, 10) != 0)
    {
        handleError("'listen' failure");
    }

    // Handling client as long as we didn't get SIGINT
    while (!receivedSIGINT)
    {
        handlingClient = 0;

        // Connecting to client
        int connFD = accept(socketFD, NULL, NULL);

        handlingClient = 1; // Server accepted the connection from client

        if (connFD < 0)
        {
            if (errno == EINTR)
                continue; // Waiting for new client

            else
            {
                handleError("'accept' failure");
            }
        }

        // Getting file size from client
        uint32_t fileSize;
        ssize_t bytesRead = readFromFD(connFD, &fileSize, sizeof(fileSize));
        if (bytesRead == -1)
            continue; // The connection from client to server terminated

        fileSize = ntohl(fileSize);

        // Getting file content from client
        char buf[fileSize];
        bytesRead = readFromFD(connFD, buf, fileSize);
        if (bytesRead == -1)
            continue; // The connection from client to server terminated

        // Calculating the PCC
        uint32_t pcc = calcPCC(buf, fileSize);
        pcc = htonl(pcc);

        // Sending result to client
        ssize_t bytesWrote = write2FD(connFD, &pcc, sizeof(pcc));
        if (bytesWrote == -1)
            continue; // The connection from server to client terminated

        close(connFD);
    }

    // Got SIGINT while handling client so we finished handling the client
    // and now we need to print and exit
    printTotalPCC();
    close(socketFD);
    return EXIT_SUCCESS;
}