#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

// Macro to handle errors
#define handleError(msg) \
    perror(msg);         \
    exit(EXIT_FAILURE);

/*
    @brief Read data of given size from file descriptor to buffer
    @param fd File descriptor to read from
    @param buf Buffer to read data to
    @param size Size of data to read
    @return number of bytes read
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
        handleError("'read' failure");
    }

    return totalRead;
}

/*
    @brief Write data of given size from buffer to file descriptor
    @param fd File descriptor to write to
    @param buf Buffer to write data from
    @param size Size of data to write
    @return number of bytes written
*/
ssize_t write2FD(int fd, void *buf, ssize_t size)
{
    ssize_t totalSent = 0;    // How much already sent
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
        handleError("'write' failure");
    }

    return totalSent;
}

/*
    @brief Calculate the file size
    @param fd File desriptor of the file that it's size need to calculate
    @return file size
*/
uint32_t fetchFileSize(int fd)
{
    // Getting to the end of the file
    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize < 0)
    {
        perror("'lseek' failure");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Returning to the beginning of the file
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        perror("'lseek' failure");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return (uint32_t)fileSize;
}

/*
    @brief Main function
    @param argc Number of command line arguments
    @param argv Command line arguments
    @return 0 on success
    @pre argv[1] is the server's IP address
    @pre argv[2] is the server's port
    @pre argv[3] is the path of the file to send
*/
int main(int argc, char *argv[])
{
    // Invalid number of arguments
    if (argc != 4)
    {
        handleError("Invalid number of arguments");
    }
    
    // Opening the file to send
    int fileFD = open(argv[3], O_RDONLY);
    if (fileFD == -1)
    {
        handleError("'open' failure");
    }

    // Opening socket
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        handleError("'socket' failure");
    }

    // Setting the IPv4, port and address of the server
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serverAddr.sin_addr) == 0)
    {
        handleError("'inet_pton' failure");
    }

    // Connecting to the server
    if (connect(socketFD, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        handleError("'connect' failure");
    }

    // Reading the file
    uint32_t fileSize = fetchFileSize(fileFD);
    char buf[fileSize];
    memset(buf, 0, fileSize);
    readFromFD(fileFD, buf, fileSize);

    // Sending file size to the sever
    uint32_t fileSizeNet = htonl(fileSize);
    write2FD(socketFD, &fileSizeNet, sizeof(fileSizeNet));

    // Sending file content to the server
    write2FD(socketFD, buf, fileSize);

    // Getting result from server
    uint32_t pcc;
    readFromFD(socketFD, &pcc, sizeof(pcc));

    // Finalizing
    printf("# of printable characters: %u\n", ntohl(pcc));
    close(socketFD);
    close(fileFD);
    return EXIT_SUCCESS;
}