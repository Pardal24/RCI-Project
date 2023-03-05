#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>

#define MAX_BUFFER_SIZE 1024

char *My_IP()
{
    int n;
    struct ifreq ifr;
    char array[] = "eth0";

    n = socket(AF_INET, SOCK_DGRAM, 0);
    // Type of address to retrieve - IPv4 IP address
    ifr.ifr_addr.sa_family = AF_INET;
    // Copy the interface name in the ifreq structure
    strncpy(ifr.ifr_name, array, IFNAMSIZ - 1);
    ioctl(n, SIOCGIFADDR, &ifr);
    close(n);
    // display result
    return (inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
}

int main(int argc, char *argv[])
{
    char *Ip, *TCP, *regIP, *regUDP, *type;
    fd_set rfds;
    int fd_listen, newfd, n;
    int *client_fds = NULL;
    struct sockaddr_in addr;
    socklen_t addrlen;
    char buffer[MAX_BUFFER_SIZE];
    int client_count = 0, maxfd = 0, counter, i;

    if (argc != 5)
    {
        printf("Number of arguments is wrong, you are wrong\n Call the program with: cot IP TCP regIP(reg193.136.138.142) regUDP(reg59000)\n");
        exit(0);
    }
    else
    {
        if (strcmp(argv[1], My_IP()) != 0) // Caso o utilizador não saiba o seu id devolver o id dele
        {
            printf("Your IP is: %s\n", My_IP());
            exit(0);
        }
        if (strcmp(argv[3], "reg193.136.138.142") != 0)
        {
            printf("Please incert in regIP: reg193.136.138.142\n");
            exit(0);
        }
        if (strcmp(argv[4], "reg59000") != 0)
        {
            printf("Please incert in regUDP: reg59000\n");
            exit(0);
        }
        Ip = argv[1];
        TCP = argv[2];
        regIP = argv[3];
        regUDP = argv[4];

        printf("Welcome to the server please select your next command from this list:\n -join net id\n -djoin net id bootid bootIP bootTCP\n"
               " -create name\n -delete name\n -get dest name\n -show topology (st)\n -show names (sn)\n -show routing (sr)\n -leave\n -exit\n");
    }

    while (1)
    {
        FD_ZERO(&rfds);

        FD_SET(fd_listen, &rfds);
        FD_SET(STDIN_FILENO, &rfds);

        for (i = 0; i < client_count; i++)
        {
            FD_SET(client_fds[i], &rfds);
            if (client_fds[i] > maxfd)
            {
                maxfd = client_fds[i];
            }
        }

        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);

        if (counter <= 0) /*error*/
            exit(1);
        while (counter > 0)
        {

            if (FD_ISSET(fd_listen, &rfds))
            {
                addrlen = sizeof(addr);
                if ((newfd = accept(fd_listen, (struct sockaddr *)&addr, &addrlen)) == -1) /*error*/
                    exit(EXIT_FAILURE);

                printf("New connection on socket %d\n", newfd);

                client_fds = (int *)realloc(client_fds, (client_count + 1) * sizeof(int));
                client_fds[client_count++] = newfd;
                counter--;
            }
            else if (FD_ISSET(STDIN_FILENO, &rfds))
            {
                FD_CLR(STDIN_FILENO, &rfds);
                if ((n = read(STDIN_FILENO, buffer, 128)) != 0)
                {
                    if (n == -1) /*error*/
                        exit(1);
                }
                else
                {
                    close(STDIN_FILENO);
                    counter--;
                }
            }
            else
            {
                for (i = 0; i < client_count; i++)
                {
                    if (FD_ISSET(client_fds[i], &rfds))
                    {
                        n = read(client_fds[i], buffer, MAX_BUFFER_SIZE);
                        if (n == -1)
                        {
                            perror("read");
                            exit(EXIT_FAILURE);
                        }
                        if (n == 0)
                        {
                            printf("Connection closed by client on socket %d\n", client_fds[i]);
                            close(client_fds[i]);
                            client_fds[i] = -1;
                        }
                        else
                        {
                            buffer[n] = '\0';
                            printf("Received message from client on socket %d: %s\n", client_fds[i], buffer);
                        }
                        counter--;
                    }
                }
            }
        }
    }

    exit(0);
}