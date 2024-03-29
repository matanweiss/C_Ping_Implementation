#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#define PORT 3000

int main(int argc, char *argv[])
{
    // creating a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket() failed");
        return 1;
    }

    struct sockaddr_in Address;
    memset(&Address, 0, sizeof(Address));

    Address.sin_family = AF_INET;
    Address.sin_addr.s_addr = INADDR_ANY;
    Address.sin_port = htons(PORT);

    // fix for "address already in use" error
    int yes = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        perror("setsockopt() failed");
        close(sock);
    }

    // Bind the socket to the port with any IP at this port
    int bindResult = bind(sock, (struct sockaddr *)&Address, sizeof(Address));
    if (bindResult == -1)
    {
        perror("bind() failed");
        close(sock);
        return -1;
    }

    // listen to better_ping
    int listenResult = listen(sock, 3);
    if (listenResult == -1)
    {
        perror("listen() failed");
        close(sock);
        return -1;
    }
    struct sockaddr_in senderAddress;
    socklen_t senderAddressLen = sizeof(senderAddress);
    memset(&senderAddress, 0, sizeof(senderAddress));
    senderAddressLen = sizeof(senderAddress);

    // accepting the connection request from better_ping
    int senderSocket = accept(sock, (struct sockaddr *)&senderAddress, &senderAddressLen);
    if (senderSocket == -1)
    {
        perror("accept() failed");
        close(sock);
        return -1;
    }

    // turn the socket into a non-blocking socket
    fcntl(sock, F_SETFL, O_NONBLOCK);
    fcntl(senderSocket, F_SETFL, O_NONBLOCK);

    int sent = 1;
    struct timeval start, end;
    double timeDelta = 0.0;

    while (timeDelta < 10.0)
    {
        // when a new packet has been sent, reset the timer
        if (recv(senderSocket, &sent, sizeof(sent), 0) > 0)
        {
            gettimeofday(&start, NULL);
        }

        // calculate time delta between the last time a packet has been sent and current time
        gettimeofday(&end, NULL);
        timeDelta = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    }

    // when the timer has passed 10 seconds, terminate the process
    close(sock);
    close(senderSocket);
    printf("server %s cannot be reached\n", argv[1]);
    kill(getppid(), SIGKILL);
    return 0;
}