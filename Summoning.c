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
#include <netdb.h>
#include <time.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_CONNECTIONS 100

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

char *udp_communication(char *IP_server, char *port, char *msg)
{
    struct addrinfo hints, *res;
    char *msg_rcv;
    int fd, errcode;
    ssize_t n;
    fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    if (fd == -1)                        /*error*/
        exit(1);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    errcode = getaddrinfo(IP_server, port, &hints, &res);
    if (errcode != 0) /*error*/
        exit(1);
    n = sendto(fd, msg, strlen(msg), 0, res->ai_addr, res->ai_addrlen); // msg = NODES 075
    if (n == -1)                                                        /*error*/
        exit(1);

    struct sockaddr addr;
    socklen_t addrlen;
    char buffer[2016];
    char host[NI_MAXHOST], service[NI_MAXSERV]; // consts in <netdb.h>
    /*...*/                                     // see previous task code
    addrlen = sizeof(addr);

    n = recvfrom(fd, buffer, 2016, 0, &addr, &addrlen);
    if (n == -1) /*error*/
        exit(1);
    buffer[n] = '\0';
    msg_rcv = buffer;
    close(fd);
    return msg_rcv;
}

// char *tcp_communication(char *IP_server, char *port, char *msg)
// {
//     struct addrinfo hints, *res;
//     int fd, n;
//     ssize_t nbytes, nleft, nwritten, nread;
//     char *ptr, buffer[128];

//     fd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
//     if (fd == -1)
//         exit(1); // error
//     memset(&hints, 0, sizeof hints);
//     hints.ai_family = AF_INET;       // IPv4
//     hints.ai_socktype = SOCK_STREAM; // TCP socket
//     n = getaddrinfo("tejo.tecnico.ulisboa.pt", "58001", &hints, &res);
//     if (n != 0) /*error*/
//         exit(1);
//     n = connect(fd, res->ai_addr, res->ai_addrlen);
//     if (n == -1) /*error*/
//         exit(1);

//     ptr = sprintf(buffer, "Hello!\n", ...); // Trocar para a mensagem que quero transmitir
//     nbytes = 7;
//     nleft = nbytes;
//     while (nleft > 0)
//     {
//         nwritten = write(fd, ptr, nleft);
//         if (nwritten <= 0) /*error*/
//             exit(1);
//         nleft -= nwritten;
//         ptr += nwritten;
//     }
//     nleft = nbytes;
//     ptr = buffer;
//     while (nleft > 0)
//     {
//         nread = read(fd, ptr, nleft);
//         if (nread == -1) /*error*/
//             exit(1);
//         else if (nread == 0)
//             break; // closed by peer
//         nleft -= nread;
//         ptr += nread;
//     }
//     nread = nbytes - nleft;
//     buffer[nread] = '\0';
//     printf("echo: %s\n", buffer);
//     close(fd);
//     // Dar return da mensagem
// }

int cmpr_id(char *node_list[], char *id, int size)
{
    int i, confirm = 0, j = 0;
    char *used_id;
    int id_list[100];

    for (i = 0; i <= 100; i++)
    {
        id_list[i] = 0;
    }
    while (j <= size && node_list[j] != NULL)
    {
        if (node_list[j] != NULL)
        {
            if (strncmp(node_list[j], id, 2) == 0)
            {
                confirm = 1;
            }
        }

        id_list[atoi(node_list[j])] = 1;
        j++;
    }
    if (confirm == 1)
    {
        for (i = 0; i < 100; i++)
        {
            if (id_list[i] == 0)
            {
                if (i < 10)
                {
                    sprintf(id, "0%d", i);
                    printf("new id %s\n", id);
                }
                else
                {
                    sprintf(id, "%d", i);
                }
                return 1;
            }
        }
    } // Item found
    else
    {
        return 0;
    }
}

void join(char *net, char *id, char *regIP, char *regUDP, char *user_ip, char *user_tcp)
{
    char msg_send[128], *msg_recv, registo[128], *ok_reg, *node_list[MAX_CONNECTIONS], *new_connect;
    char *line, rdm_node[128];
    struct Msg
    {
        char msg_send[128];
        char *msg_recv
    };

    struct Nodeinfo
    {
        char *n_id;
        char *n_ip;
        char *n_port;
    };
    struct Msg nodes;
    struct Msg new_tcp;
    struct Nodeinfo nodeinfo;

    int i = 0, index, size = (sizeof(node_list) / sizeof(node_list[0]));
    sprintf(nodes.msg_send, "NODES %s", net);
    nodes.msg_recv = udp_communication(regIP, regUDP, msg_send);
    printf("%s\n", nodes.msg_recv);

    memset(node_list, '\0', sizeof(node_list));

    i = 0;
    line = strtok(nodes.msg_recv, "\n");
    line = strtok(NULL, "\n");
    if (line != NULL)
    {
        while (line != NULL && i < MAX_CONNECTIONS)
        {
            node_list[i] = line;
            line = strtok(NULL, "\n");
            i++;
        }

        if (cmpr_id(node_list, id, size) == 1)
        {
            printf("There is already your id, your new id is: %s\n", id);
        }
        else
        {
            printf("Ready to connect\n");
        }

        index = rand() % (i + 1);
        strcpy(rdm_node, node_list[index]);
        printf("%s\n", rdm_node);

        nodeinfo.n_id = strtok(rdm_node, " ");
        nodeinfo.n_ip = strtok(NULL, " ");
        nodeinfo.n_port = strtok(NULL, "\n");

        sprintf(new_tcp.msg_send, "NEW %s %s %s", nodeinfo.n_id, nodeinfo.n_ip, nodeinfo.n_port);
        new_connect = tcp_communication()
    }
    // FAZER A LIGACAO ENTRE OS NOS
    // INCLUI RECEBER O EXTERNO E METER COMO NOSSO BACKUP

    // sprintf(registo, "REG %s %s %s %s", net, id, user_ip, user_tcp);
    // printf("%s\n", registo);
    // ok_reg = udp_communication(regIP, regUDP, registo);
    // printf("%s\n", ok_reg);
}

void djoin(char *net, char *id, char *regIP, char *regUDP, char *user_ip, char *user_tcp) {}

// int leave()
// {

//     // 1 - apagar o seu contacto do servidor de nós
//     // UNREG net id - mensagem de retiro da rede
//     // receber OKUNREG do servidor
//     // 2 - terminar as sessões TCP com todos os seus vizinhos (há lista de vizinhos)?
// }

void commands(char *input, char *reg_ip, char *reg_udp, char *user_ip, char *user_tcp)
{
    char *action, *net, id[128];
    memset(id, 0, sizeof(id));
    action = strtok(input, " ");

    if (strcmp(action, "join") == 0)
    {
        net = strtok(NULL, " ");
        strcpy(id, strtok(NULL, " \n"));
        join(net, id, reg_ip, reg_udp, user_ip, user_tcp);
    }
    else if (strcmp(action, "djoin") == 0)
    {
    }
}

int tcp_listener(char *port)
{
    int port_tcp = atoi(port);
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_tcp);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(fd, SOMAXCONN) == -1)
    {
        perror("Error listening on socket");
        exit(EXIT_FAILURE);
    }

    return fd;
}

int main(int argc, char *argv[])
{
    char *Ip, *type, *reg_ip, *action, *net, *bootIP, *name, *reg_udp, *TCP_port; // Passar mrds para estruturas
    char buffer[MAX_BUFFER_SIZE];
    int fd_listen, newfd, n;
    int client_count = 0, maxfd = 0, counter, i, id, bootid, bootTCP;
    int *client_fds = NULL; // vetor de clientes
    struct sockaddr_in addr;
    fd_set rfds;
    socklen_t addrlen;
    time_t t;

    srand((unsigned)time(&t));

    if (argc != 5)
    {
        printf("Number of arguments is wrong, you are wrong\n Call the program with: IP TCP regIP(193.136.138.142) regUDP(59000)\n");
        exit(0);
    }

    else
    {
        Ip = argv[1];
        TCP_port = argv[2];
        reg_ip = argv[3];
        reg_udp = argv[4];

        if (strcmp(argv[1], My_IP()) != 0) // Caso o utilizador não saiba o seu id devolver o id dele
        {
            printf("Your IP is: %s\n", My_IP()); // ver qual o ip a usar
        }

        if (strcmp(reg_ip, "193.136.138.142") != 0)
        {
            printf("Please incert in regIP: 193.136.138.142\n");
            exit(0);
        }

        if (strcmp(argv[4], "59000") != 0)
        {
            printf("Please incert in regUDP: 59000\n");
            exit(0);
        }

        printf("Welcome to the server please select your next command from this list:\n -join net id\n -djoin net id bootid bootIP bootTCP\n"
               " -create name\n -delete name\n -get dest name\n -show topology (st)\n -show names (sn)\n -show routing (sr)\n -leave\n -exit\n");
    }

    fd_listen = tcp_listener(TCP_port);

    while (1)
    {
        FD_ZERO(&rfds); // clear ao rfds

        FD_SET(fd_listen, &rfds);    // vigia porta listen
        FD_SET(STDIN_FILENO, &rfds); // vigia terminal

        for (i = 0; i < client_count; i++) // vigia canais TCP atuais
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
                FD_CLR(fd_listen, &rfds);
                addrlen = sizeof(addr);
                if ((newfd = accept(fd_listen, (struct sockaddr *)&addr, &addrlen)) == -1) /*error*/
                    exit(EXIT_FAILURE);

                printf("New connection on socket %d\n", newfd);

                client_fds = (int *)realloc(client_fds, (client_count + 1) * sizeof(int));
                client_fds[client_count++] = newfd;
                // falta comparar com o maxfd
                counter--;
            }
            else if (FD_ISSET(STDIN_FILENO, &rfds))
            {
                FD_CLR(STDIN_FILENO, &rfds);
                if ((n = read(STDIN_FILENO, buffer, 128)) == 0)
                {
                    if (n == -1)
                    { /*error*/
                        exit(1);
                    }
                }
                else
                {
                    commands(buffer, reg_ip, reg_udp, Ip, TCP_port);
                    // action = strtok(buffer, " ");

                    // if (strcmp(action, "join") == 0)
                    // {
                    //     action = strtok(NULL, " ");

                    //     if (strcmp(action, "net") == 0)
                    //     {
                    //         action = strtok(NULL, " ");
                    //         if (strcmp(action, "id") == 0)
                    //         {
                    //             join(net, id, reg_ip, reg_udp); // Falta ainda perceber onde raio vai perguntar sobre os nos que estao la
                    //         }
                    //         // else if (strcmp(action, "djoin") == 0)
                    //         // {
                    //         //     scanf("%s %d %d %s %d", net, &id, &bootid, bootIP, &bootTCP);
                    //         //     djoin(net, id, 0, 0);
                    //         // }
                    //         // else if (strcmp(action, "create") == 0)
                    //         // {
                    //         //     scanf("%s", name);
                    //         // }
                    //         // else
                    //         // {
                    //         //     printf("Invalid input\n");
                    //         // }
                    close(STDIN_FILENO);
                    counter--;
                }
            }
            else
            {
                for (; counter > 0; counter--)
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
    }

    return 0;
}
