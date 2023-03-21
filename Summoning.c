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

typedef struct Node
{
    int id;
    char ip[30];
    char port[10];
    int fd;
} Node;

typedef struct My_Node
{
    Node myinfo;
    Node internos[100];
    Node externo;
    Node backup;
    int net;
} My_Node;

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
    if (n == -1)
    { /*error*/
        exit(1);
    }
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

    if (write(fd, msg, strlen(msg)) == -1)
    {
        printf("error: %s\n", strerror(errno));
        exit(1);
    }

    return fd;
}

void read_msg(Node *tempNode, My_Node *my_node, char *buffer) // Vai ler a mensagem recebida pelo nó
{
    Node extern_Node;
    memset(&extern_Node, 0, sizeof(extern_Node));

    if (sscanf(buffer, "NEW %d %s %s", &tempNode->id, tempNode->ip, tempNode->port) == 3)
    {
        printf("Mensagem recebida: %s\n", buffer);
        memset(buffer, 0, strlen(buffer));
        if (my_node->externo.id == my_node->myinfo.id) // se o meu externo for eu próprio -> tou sozinho -> digo ao novo nó que sou o externo dele & ele é o meu externo
        {
            my_node->externo = *tempNode;

            sprintf(buffer, "EXTERN %02d %s %s", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
            printf("Mensagem envida: %s\n", buffer);

            if (write(tempNode->fd, buffer, strlen(buffer)) == -1)
            {
                printf("error: %s\n", strerror(errno));
                exit(1);
            } // envio o meu externo para novo o nó colocar como o seu backup
        }
        else // se o meu externo não for eu próprio -> já tenho externo -> informo o novo nó o meu externo (o backup dele) e guardo-o como interno
        {
            my_node->internos[tempNode->id] = *tempNode;

            sprintf(buffer, "EXTERN %02d %s %s", my_node->externo.id, my_node->externo.ip, my_node->externo.port); // envio o meu externo para novo o nó colocar como o seu backup
            printf("Mensagem envida: %s\n", buffer);
            if (write(tempNode->fd, buffer, strlen(buffer)) == -1)
            {
                printf("error: %s\n", strerror(errno));
                exit(1);
            }
        }
        memset(buffer, 0, strlen(buffer));

    } // registro o novo nó nos meus dados

    if (sscanf(buffer, "EXTERN %d %s %s", &extern_Node.id, extern_Node.ip, extern_Node.port) == 3)
    {
        printf("Mensagem recebida: %s\n", buffer);
        if (tempNode->id == extern_Node.id) // se o id do nó que me envia o externo for igual ao id me enviado entao o meu externo será o no que comunicou comigo
        {
            my_node->externo = *tempNode; // o meu externo é aquele que me enviou a mensagem
            printf("My extern is: %02d %s %s\n", tempNode->id, tempNode->ip, tempNode->port);
            memset(buffer, 0, strlen(buffer));
        }
        else
        {
            my_node->externo = *tempNode;
            my_node->backup = extern_Node;
            printf("My extern is: %02d %s %s\n", tempNode->id, tempNode->ip, tempNode->port);
            printf("My backup is: %02d %s %s\n", extern_Node.id, extern_Node.ip, extern_Node.port);
            memset(buffer, 0, strlen(buffer));
        }
    }
}

int cmpr_id(char *node_list[], My_Node *my_node, int size)
{

    int i, confirm = 0, j = 0, id;
    int id_list[100];

    memset(id_list, 0, sizeof(id_list));

    while (j <= size && node_list[j] != NULL)
    {
        if (node_list[j] != NULL)
        {
            sscanf(node_list[j], "%d ", &id);
            if (id == my_node->myinfo.id)
            {
                confirm = 1;
            }
            // if (strncmp(node_list[j], my_node->myinfo.id, 2) == 0)
            // {
            //     confirm = 1;
            // }
        }

        id_list[id] = 1;
        j++;
    }
    if (confirm == 1)
    {
        for (i = 0; i < 100; i++)
        {
            if (id_list[i] == 0)
            {
                my_node->myinfo.id = i;
                return 1;
            }
        }
    } // Item found

    return 0;
}

void join(char *regIP, char *regUDP, My_Node *my_node, Node *tempNode)
{
    char registo[128], *ok_reg, *node_list[MAX_CONNECTIONS];
    char *line, rdm_node[128];

    struct Msg
    {
        char send[128];
        char recv[3000]; // tamanho maximo que o nodes list pode retornar
    };

    struct Msg msg;
    struct Msg new_tcp;
    int i = 0, index, size = (sizeof(node_list) / sizeof(node_list[0]));

    sprintf(msg.send, "NODES %03d", my_node->net);
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
        if (cmpr_id(node_list, my_node, size) == 1)
        {
            printf("There is already your id, your new id is: %02d\n", my_node->myinfo.id);
            my_node->externo.id = my_node->myinfo.id;
            my_node->backup.id = my_node->myinfo.id;
        }
        else
        {
            printf("Ready to connect\n");
        }

        index = rand() % i;
        printf("Sou o i: %d\n", i);
        strcpy(rdm_node, node_list[index]);
        printf("%s\n", rdm_node);

        sscanf(rdm_node, "%d %s %s", &tempNode->id, tempNode->ip, tempNode->port);

        sprintf(new_tcp.send, "NEW %02d %s %s", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
        printf("Mensagem enviada: %s -----> id %02d\n", new_tcp.send, tempNode->id);
        tempNode->fd = tcp_communication(tempNode->ip, tempNode->port, new_tcp.send);
    }

    sprintf(registo, "REG %03d %02d %s %s", my_node->net, my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
    printf("%s\n", registo);
    ok_reg = udp_communication(regIP, regUDP, registo);
    printf("%s\n", ok_reg);
}

void djoin(My_Node *my_node, Node *tempNode)
{
    char msg_send[128];

    memset(msg_send, 0, sizeof(msg_send));

    sprintf(msg_send, "NEW %02d %s %s", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
    printf("Mensagem enviada: %s\n", msg_send);
    tempNode->fd = tcp_communication(tempNode->ip, tempNode->port, msg_send);
}

void leave(Node *leaving_node, My_Node *my_node, Node *tempNode)
{
    char msg_send[128];
    memset(msg_send, 0, sizeof(msg_send));

    printf("Connection closed by client on socket %d\n", leaving_node->fd);
    if (my_node->externo.id == leaving_node->id) // externo deu leave -> meto como externo o meu backup
    {
        memset(&my_node->externo, 0, sizeof(my_node->externo));

        if (my_node->myinfo.id == my_node->backup.id) // sou ancora, logo o meu externo que saiu, tambem era ancora -> tenho de eleger outra ancora
        {
            // escolher um interno para ser ancora
            // mandar uma mensagem "EXTERN" para ele (ele vai receber e atualizar verificar que recebeu uma mensagem do externo )
            //  O nó que receber esta mensagem vai perceber que recebeu um nó de backup que é igual a si proprio
            //  logo fui eleito ancora, -> o meu externo mantém-se, passo a ser o meu próprio backup, e atualizo o backup dos meus internos

            // Mandar msg para os meus internos a informar do novo backup deles.
            // for(i=0; i<100; i++)
            // if(My_node->interno[i].fd!=0);
            //  My_node->externo = My_node->interno[i];
            //  memset(my_node->interno[i]);
            //  break;´
            // manda Extern para todos os vizinhos

            // mandar extern de mim mesmo para um dos meus internos;
            sprintf(msg_send, "EXTERN %02d %s %s", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
        }
        sprintf(msg_send, "NEW %02d %s %s", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
        printf("Mensagem enviada: %s -----> id %02d\n", msg_send, my_node->backup.id);
        tempNode->fd = tcp_communication(my_node->backup.ip, my_node->backup.port, msg_send);

        memset(msg_send, 0, sizeof(msg_send));
        sprintf(msg_send, "EXTERN %02d %s %s", my_node->backup.id, my_node->backup.ip, my_node->backup.port); // mando info do meu novo externo aos internos

        for (int i = 0; i < 100; i++)
        {
            if (my_node->internos[i].fd != 0)
            {
                if (write(my_node->internos[i].fd, msg_send, strlen(msg_send)) == -1)
                {
                    printf("error: %s\n", strerror(errno));
                    exit(1);
                }
            }
        }
    }
    else // interno deu leave -> fecha apenas a porta com esse interno
    {
        memset(&my_node->internos[leaving_node->id], 0, sizeof(my_node->internos[leaving_node->id]));
    }
}

void commands(char *input, char *reg_ip, char *reg_udp, My_Node *my_node, Node *tempNode)
{

    if (sscanf(input, "join %d %d", &my_node->net, &my_node->myinfo.id))
    {
        my_node->externo = my_node->myinfo;
        my_node->backup = my_node->myinfo;
        join(reg_ip, reg_udp, my_node, tempNode);
    }
    else if (sscanf(input, "djoin %d %d %d %s %s", &my_node->net, &my_node->myinfo.id, &tempNode->id, tempNode->ip, tempNode->port))
    {
        if (my_node->myinfo.id == tempNode->id)
        {
            my_node->externo = my_node->myinfo;
            my_node->backup = my_node->myinfo;
            printf("Sou o primeiro nó");
        }
        else
        {
            my_node->externo = my_node->myinfo;
            my_node->backup = my_node->myinfo;
            djoin(my_node, tempNode);
        }
    }
    else if (strcmp(input, "st\n") == 0)
    {
        printf("Meus internos são:\n");
        for (int i = 0; i < 100; i++)
        {
            if (my_node->internos[i].fd != 0)
            {
                printf("%d %s %s\n", my_node->internos[i].id, my_node->internos[i].ip, my_node->internos[i].port);
            }
        }
        printf("O meu externo é : %d %s %s\n", my_node->externo.id, my_node->externo.ip, my_node->externo.port);
        printf("O meu backup é : %d %s %s\n", my_node->backup.id, my_node->backup.ip, my_node->backup.port);
    }
    else if (strcmp(input, "leave\n") == 0)
    {
        char registo[128], ok_reg[128];

        memset(registo, 0, sizeof(registo));
        memset(ok_reg, 0, sizeof(ok_reg));

        sprintf(registo, "UNREG %03d %02d", my_node->net, my_node->myinfo.id);
        printf("%s\n", registo);
        strcpy(ok_reg, udp_communication(reg_ip, reg_udp, registo));
        printf("%s\n", ok_reg);

        for (int i = 0; i < 100; i++)
        {
            if (my_node->internos[i].fd != 0)
            {
                close(my_node->internos[i].fd);
            }
        }
        close(my_node->externo.fd);
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
    struct sockaddr_in addr;

    if (fd == -1)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

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
    char *type, *action, *bootIP, *name;
    char buffer[MAX_BUFFER_SIZE];
    int newfd, n;
    int client_count = 0, maxfd = 0, counter, i, j, id;
    struct sockaddr_in addr;
    fd_set rfds;
    socklen_t addrlen;

    time_t t;
    srand((unsigned)time(&t));

    char reg_ip[30];
    char reg_udp[6];
    // nossas estruturas
    My_Node my_node;
    Node tempNode;
    Node clients[100];

    memset(&my_node, 0, sizeof(my_node));
    memset(&clients, 0, sizeof(clients));
    memset(buffer, 0, sizeof(buffer));
    memset(&my_node.internos, 0, sizeof(my_node.internos));

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

    my_node.myinfo.fd = tcp_listener(my_node.myinfo.port); // abertura de uma porta tcp para listen
    maxfd = my_node.myinfo.fd;

    while (1)
    {
        FD_ZERO(&rfds); // clear ao rfds

        memset(&tempNode, 0, sizeof(tempNode));

        FD_SET(my_node.myinfo.fd, &rfds); // vigia porta listen
        FD_SET(STDIN_FILENO, &rfds);      // vigia terminal

        for (i = 0; i < client_count; i++) // vigia canais TCP atuais
        {
            FD_SET(clients[i].fd, &rfds);
            if (clients[i].fd > maxfd)
            {
                maxfd = clients[i].fd;
            }
        }

        printf("Tou a espera\n");
        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);

        if (counter <= 0) /*error*/
            exit(1);

        while (counter > 0)
        {

            if (FD_ISSET(my_node.myinfo.fd, &rfds)) // Notificacao no meu server TCP // o unico caso que isto acontece é a receber uma msg "NEW IP TCP"
            {
                FD_CLR(my_node.myinfo.fd, &rfds);
                addrlen = sizeof(addr);
                if ((newfd = accept(my_node.myinfo.fd, (struct sockaddr *)&addr, &addrlen)) == -1) /*error*/
                    exit(EXIT_FAILURE);

                clients[client_count].fd = newfd;
                if (maxfd < newfd)
                {
                    maxfd = newfd;
                }
                printf("Client count: %d\n", client_count);
                client_count++;
                counter--;
            }
            else if (FD_ISSET(STDIN_FILENO, &rfds)) // Notificao no user input
            {
                FD_CLR(STDIN_FILENO, &rfds);
                memset(buffer, 0, sizeof(buffer));

                if ((n = read(STDIN_FILENO, buffer, 128)) < 0)
                    exit(1);
                else
                {
                    buffer[n] = '\0';
                    commands(buffer, reg_ip, reg_udp, &my_node, &tempNode);
                    if (tempNode.fd != 0)
                    {
                        clients[client_count++] = tempNode;
                        printf("Client_count: %d\n", client_count);
                        for (int i = 0; i < client_count; i++)
                        {
                            if (clients[i].fd > maxfd)
                            {
                                maxfd = clients[i].fd;
                            }
                        }
                    }
                }
                counter--;
            }
            else // nao mexi aqui
            {
                for (; counter > 0; counter--)
                {
                    for (i = 0; i < client_count; i++)
                    {
                        if (FD_ISSET(clients[i].fd, &rfds))
                        {
                            memset(buffer, 0, sizeof(buffer));
                            n = read(clients[i].fd, buffer, MAX_BUFFER_SIZE);
                            if (n <= -1)
                            {
                                perror("read");
                                exit(EXIT_FAILURE);
                            }
                            if (n == 0)
                            {
                                // funcao leave recebe como argumentos o nó que sai, a info do meu nó e o possível nó que terei de me juntar
                                leave(&(clients[i]), &my_node, &tempNode);
                                close(clients[i].fd);                       // garanto que a conexão do nó que saiu está fechada
                                memset(&clients[i], 0, sizeof(clients[i])); // limpo toda a informação guardada pelo nó no array
                                for (j = client_count; j >= 0; j--)         // organização da lista de nós
                                {
                                    if (clients[client_count].fd == 0) // Se no index do client_count dos clients tiver a 0, foi esse nó que saiu não é preciso mais nada
                                    {
                                        break;
                                    }
                                    else // Se o nó que saiu tiver a meio do array, meter o último nó do array e meter no lugar do que saiu
                                    {
                                        if (clients[j].fd == 0)
                                        {
                                            clients[j] = clients[client_count];
                                            memset(&clients[client_count], 0, sizeof(clients[client_count])); // limpar o lugar do último nó que foi reorganizado
                                        }
                                    }
                                }
                                client_count--;
                                if (tempNode.fd != 0) // Aqueles a que se ligaram a um novo externo precisam de adicionar um novo nó a lista
                                {
                                    clients[client_count++] = tempNode;
                                    printf("Client_count: %d\n", client_count);
                                    for (int i = 0; i < client_count; i++)
                                    {
                                        if (clients[i].fd > maxfd)
                                        {
                                            maxfd = clients[i].fd;
                                        }
                                    }
                                }
                                maxfd = 0; // procuramos o novo maxfd
                                for (int i = 0; i < client_count; i++)
                                {
                                    if (clients[i].fd > maxfd)
                                    {
                                        maxfd = clients[i].fd;
                                    }
                                }
                            }
                            else
                            {
                                buffer[n] = '\0';
                                printf("My id: %d\n", my_node.myinfo.id);
                                printf("My extern: %d\n", my_node.externo.id);
                                read_msg(&(clients[i]), &my_node, buffer); // fd do no a enviar mensagem, mensagem a ler
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