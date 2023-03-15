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

typedef struct other_node
{
    int id;
    char ip[128];
    char port[10];
    int fd;
} other_node;

typedef struct APP
{
    My_info myinfo;
    other_node internos[100];
    other_node externo;
    other_node backup;
    int net;
} APP;

typedef struct My_info
{
    int id;
    char ip[128];
    char port[8];

} My_info;

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

int tcp_communication(char *n_ip, char *n_port, char *msg) // Vou ter de dar return do fd a qual me vou ligar e adicionar aos meus vizinhos e enfins
{
    struct addrinfo hints, *res;
    int fd, n;
    ssize_t nbytes, nleft, nwritten, nread;

    fd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
    if (fd == -1)
        exit(1); // error
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    n = getaddrinfo(n_ip, n_port, &hints, &res);
    if (n != 0) /*error*/
        exit(1);
    n = connect(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1) /*error*/
        exit(1);

    printf("%s", msg);
    if (write(fd, msg, strlen(msg)) == -1)
    {
        printf("error: %s\n", strerror(errno));
        exit(1);
    }

    close(fd);
    printf("tou a sair do tcp_communication\n");

    return fd;
}

int read_msg(int *fd, APP *my_node, char *msg_recv) // Vai ler a mensagem recebida pelo nó
{
    char *type;
    struct Nodeinfo outsider;
    // vizinhos
    // eu
    // outro
    type = strtok(msg_recv, " ");

    if (strcmp(type, "NEW") == 0)
    {
    }
    return 0;
    // Dividir a mensagem recebida com strtok
    //  Se for NEW enviar externo
}

int cmpr_id(char *node_list[], APP *my_node, int size)
{
    int i, confirm = 0, j = 0, id;
    int id_list[100];

    memset(id_list, 0, sizeof(id_list));

    while (j <= size && node_list[j] != NULL)
    {
        if (node_list[j] != NULL)
        {
            sscanf(node_list[j], "%d ", id);
            if (id == my_node->myinfo.id)
            {
                confirm = 1;
            }
            // if (strncmp(node_list[j], my_node->myinfo.id, 2) == 0)
            // {
            //     confirm = 1;
            // }
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
                    sprintf(my_node->myinfo.id, "0%d", i);
                    printf("new id %s\n", my_node->myinfo.id);
                }
                else
                {
                    sprintf(my_node->myinfo.id, "%d", i);
                }
                return 1;
            }
        }
    } // Item found

    return 0;
}

int join(int net, int id, char *regIP, char *regUDP, APP *my_node)
{
    char registo[128], *ok_reg, *node_list[MAX_CONNECTIONS];
    char *line, rdm_node[128], *ptr, *n_id, *n_ip, *n_port;
    int new_fd;

    struct Msg
    {
        char send[128];
        char recv[3000]; // tamanho maximo que o nodes list pode retornar
    };

    struct Msg msg;
    struct Msg new_tcp;
    int i = 0, index, size = (sizeof(node_list) / sizeof(node_list[0]));

    sprintf(msg.send, "NODES %03d", net);
    strcpy(msg.recv, udp_communication(regIP, regUDP, msg.send)); // udp_communication2(regIP, regUDP, msg) msg.send e msg.recv
    printf("%s\n", msg.recv);

    memset(node_list, '\0', sizeof(node_list));

    line = strtok(msg.recv, "\n");
    line = strtok(NULL, "\n");
    if (line != NULL)
    {
        while (line != NULL && i < MAX_CONNECTIONS)
        {
            node_list[i] = line;
            line = strtok(NULL, "\n");
            i++;
        }
        if (cmpr_id(node_list, my_node->myinfo.id, size) == 1)
        {
            printf("There is already your id, your new id is: %s\n", my_node->myinfo.id);
        }
        else
        {
            printf("Ready to connect\n");
        }

        index = rand() % i;
        strcpy(rdm_node, node_list[index]);
        printf("%s\n", rdm_node);

        n_id = strtok(rdm_node, " ");
        n_ip = strtok(NULL, " ");
        n_port = strtok(NULL, "\n");

        sprintf(new_tcp.send, "NEW %s %s %s", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
        printf("%s\n", new_tcp.send);
        // ptr = &new_tcp.send[0]; // ptr tem de apontar para a mensagem
        new_fd = tcp_communication(n_ip, n_port, new_tcp.send);
        return new_fd;
    }
    // FAZER A LIGACAO ENTRE OS NOS
    // INCLUI RECEBER O EXTERNO E METER COMO NOSSO BACKUP

    sprintf(registo, "REG %03d %s %s %s", net, my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
    printf("%s\n", registo);
    ok_reg = udp_communication(regIP, regUDP, registo);
    printf("%s\n", ok_reg);
}

// void djoin(char *net, char *id, char *bootid, char *bootip, char *boottcp, char *my_ip, char *my_tcp)
// {
//     char *send, *recv, *new_connect;
//     sprintf(send, "NEW %s %s %s", id, my_ip, my_tcp);
//     new_connect = tcp_communication(bootip, boottcp, send);
// }

// int leave()
// {

//     // 1 - apagar o seu contacto do servidor de nós
//     // UNREG net id - mensagem de retiro da rede
//     // receber OKUNREG do servidor
//     // 2 - terminar as sessões TCP com todos os seus vizinhos (há lista de vizinhos)?
// }

void commands(char *input, char *reg_ip, char *reg_udp, APP *my_node)
{
    char action[10]; // *net, id[128];
    int net, id;
    // memset(id, 0, sizeof(id));
    memset(action, 0, strlen(action));

    sscanf(input, "%s ", action);
    // action = strtok(input, " ");

    if (strcmp(action, "join") == 0)
    {
        sscanf(input, "join %d %d", &net, &id);
        // net = strtok(NULL, " ");
        my_node->myinfo.id = id;
        my_node->net = net;

        strcpy(my_node->myinfo.id, strtok(NULL, " \n"));
        join(net, id, reg_ip, reg_udp, &my_node);
    }
    else if (strcmp(action, "djoin") == 0)
    {
        // net = strtok(NULL, " ");
        // strcpy(id, strtok(NULL, " "));
        // djoin_arg.bootid = strtok(NULL, " ");
        // djoin_arg.bootip = strktok(NULL, " ");
        // djoin_arg.boottcp = strtok(NULL, " \n");
        // djoin(net, id, djoin_arg.bootip, djoin_arg.bootid, djoin_arg.boottcp);
    }
    // else if (strcmp(action, "leave" && net != NULL) == 0)
    // {

    //     printf("UNREG %s %s\n", net, id);
    //     // leaveNetwork(); //UNREG NET ID
    //     net = NULL;
    //     id = NULL;
    // }
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
    char *type, *action, *bootIP, *name; // Passar mrds para estruturas

    int fd_listen, newfd, n;
    int client_count = 0, maxfd = 0, counter, i, id;
    int *client_fds = NULL; // vetor de clientes
    struct sockaddr_in addr;
    fd_set rfds;
    socklen_t addrlen;

    time_t t;
    srand((unsigned)time(&t));

    char Ip[16];
    char TCP_port[6];
    char reg_ip[30];
    char reg_udp[6];

    APP my_node;
    memset(&my_node, 0, sizeof(my_node.internos));

    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    if (argc != 5)
    {
        printf("Number of arguments is wrong, you are wrong\n Call the program with: IP TCP regIP(193.136.138.142) regUDP(59000)\n");
        exit(0);
    }
    else
    {
        if (strcmp(argv[1], My_IP()) != 0) // Caso o utilizador não saiba o seu id devolver o id dele
        {
            printf("Your IP is: %s\n", My_IP()); // ver qual o ip a usar
        }

        if (strcmp(argv[3], "193.136.138.142") != 0)
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
    strcpy(reg_ip, argv[3]);
    strcpy(reg_udp, argv[4]);

    // setup da info do meu nó e abertura do TCP Server
    strcpy(my_node.myinfo.ip, argv[1]);
    strcpy(my_node.myinfo.port, argv[2]);
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

            if (FD_ISSET(fd_listen, &rfds)) // Notificacao no meu server TCP
            {
                FD_CLR(fd_listen, &rfds);
                addrlen = sizeof(addr);
                if ((newfd = accept(fd_listen, (struct sockaddr *)&addr, &addrlen)) == -1) /*error*/
                    exit(EXIT_FAILURE);

                printf("New connection on socket %d\n", newfd);

                client_fds[client_count++] = newfd;
                if (maxfd < newfd)
                {
                    maxfd = newfd;
                }
                counter--;
            }
            else if (FD_ISSET(STDIN_FILENO, &rfds)) // Notificao no user input
            {
                FD_CLR(STDIN_FILENO, &rfds);
                if ((n = read(STDIN_FILENO, buffer, 128)) < 0)
                    exit(1);
                else
                {
                    commands(buffer, reg_ip, reg_udp, &my_node);
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
                            if (n <= -1)
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
                                n = read_msg(&(client_fds[i]), &my_node, buffer); // fd do no a enviar mensagem, mensagem a ler
                                // dividir a mensagem recebida se for NEW ... -> meter dentro de uma funcao
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
