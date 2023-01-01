#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#define IP_ADDRESS "127.0.0.1"
#define PORT 3000
#define IP4_HDRLEN 20
#define ICMP_HDRLEN 8

// Compute checksum (RFC 1071).
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}

int sendPing(int sock, int seq, char *destinationIP)
{

    struct icmp icmphdr; // ICMP-header
    char data[IP_MAXPACKET] = "This is the ping.\n";

    int datalen = strlen(data) + 1;

    //===================
    // ICMP header
    //===================

    // Message Type (8 bits): ICMP_ECHO_REQUEST
    icmphdr.icmp_type = ICMP_ECHO;

    // Message Code (8 bits): echo request
    icmphdr.icmp_code = 0;

    // Identifier (16 bits): some number to trace the response.
    // It will be copied to the response packet and used to map response to the request sent earlier.
    // Thus, it serves as a Transaction-ID when we need to make "ping"
    icmphdr.icmp_id = 18;

    // Sequence Number (16 bits): starts at 0
    icmphdr.icmp_seq = seq;

    // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
    icmphdr.icmp_cksum = 0;

    // Combine the packet
    char packet[IP_MAXPACKET];

    // Next, ICMP header
    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    // After ICMP header, add the ICMP data.
    memcpy(packet + ICMP_HDRLEN, data, datalen);

    // Calculate the ICMP header checksum
    icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;

    dest_in.sin_addr.s_addr = inet_addr(destinationIP);

    struct timeval start, end;
    gettimeofday(&start, 0);
    sleep((seq + 1) * 3);

    int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));
    if (bytes_sent == -1)
    {
        fprintf(stderr, "sendto() failed with error: %d", errno);
        return -1;
    }

    // Get the ping response
    bzero(packet, IP_MAXPACKET);
    socklen_t len = sizeof(dest_in);
    ssize_t bytes_received = -1;
    while ((bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len)))
    {
        if (bytes_received > 0)
        {
            // Check the IP header
            struct iphdr *iphdr = (struct iphdr *)packet;
            struct icmphdr *icmphdr = (struct icmphdr *)(packet + (iphdr->ihl * 4));
            // printf("%ld bytes from %s\n", bytes_received, inet_ntoa(dest_in.sin_addr));
            // icmphdr->type
            char sourceIPAddrReadable[32] = {'\0'};
            inet_ntop(AF_INET, &iphdr->saddr, sourceIPAddrReadable, sizeof(sourceIPAddrReadable));
            printf("Packet IP: %s \n", sourceIPAddrReadable);
            printf("Sequence number: [%d]\n", icmphdr->un.echo.sequence);
            // printf("Successfuly received one packet with %d bytes : data length : %d , icmp header : %d , ip header : %d \n", bytes_received, datalen, ICMP_HDRLEN, IP4_HDRLEN);

            break;
        }
    }
    gettimeofday(&end, 0);

    char reply[IP_MAXPACKET];
    memcpy(reply, packet + ICMP_HDRLEN + IP4_HDRLEN, datalen);

    float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
    unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
    printf("RTT: %f milliseconds (%ld microseconds)\n\n", milliseconds, microseconds);
}

int main(int argc, char *argv[])
{
    char *args[2];
    args[0] = "./watchdog";
    if (argc != 2)
    {
        printf("usage: %s <addr> \n", argv[0]);
        exit(0);
    }
    args[1] = argv[1];
    int status;
    int pid = fork();
    if (pid == 0)
    {
        printf("in child \n");
        execvp(args[0], args);
    }
    int rawSocket = -1;
    if ((rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }

    int seq = 0;

    // creating the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("socket() failed");
        return -1;
    }

    struct sockaddr_in Address;
    memset(&Address, 0, sizeof(Address));

    Address.sin_family = AF_INET;
    Address.sin_port = htons(PORT);
    int inetResult = inet_pton(AF_INET, (const char *)IP_ADDRESS, &Address.sin_addr);
    if (inetResult <= 0)
    {
        perror("inet_pton() failed");
        return -1;
    }
    sleep(1);
    // connect to receiver
    int connectResult = connect(sock, (struct sockaddr *)&Address, sizeof(Address));
    if (connectResult == -1)
    {
        perror("connect() failed");
        close(sock);
        return -1;
    }

    int sent = 1;
    int stop = 0;
    while (1)
    {
        if (send(sock, &sent, sizeof(sent), 0) == -1)
        {
            perror("send() failed");
            return -1;
        }
        sendPing(rawSocket, seq++, argv[1]);
    }
    wait(&status); // waiting for child to finish before exiting
    printf("child exit status is: %d", status);
    close(sock);
    close(rawSocket);
    return 0;
}
