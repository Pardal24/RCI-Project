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
#define MAX_NAME_LEN 100
#define MAX_NAMES 100

typedef struct Node
{
    int id;
    char ip[30];
    char port[10];
    char buffer[128];
    int fd;
} Node;

typedef struct My_Node
{
    Node myinfo;
    Node internos[MAX_CONNECTIONS];
    Node externo;
    Node backup;
    int tabela[MAX_CONNECTIONS]; // index é o dest o conteudo desse index o vizinho pelo que se chega
    char list[MAX_NAMES][MAX_NAME_LEN];
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
    if (n == -1)
    { /*error*/
        printf("Error connecting to node\n");
        return 0;
    }

    if (write(fd, msg, strlen(msg)) == -1)
    {
        printf("error: %s\n", strerror(errno));
        return 0;
    }

    return fd;
}

void write_to_nb(My_Node *my_node, int vizinho, char msg[128]) // Mandar mensagem se o destino já estiver na tabela
{

    if (my_node->externo.id == vizinho)
    {
        if (write(my_node->externo.fd, msg, strlen(msg)) == -1)
        {
            printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
            return;
        }
    }
    else
    {
        if (write(my_node->internos[vizinho].fd, msg, strlen(msg)) == -1)
        {
            printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
            return;
        }
    }
}

void write_to_all(My_Node *my_node, char msg[128], int recv_from)
{
    if (my_node->externo.id != recv_from)
    {
        if (write(my_node->externo.fd, msg, strlen(msg)) == -1) // mandar para o externo
        {
            printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
            return;
        }
    }
    for (int i = 0; i < MAX_CONNECTIONS; i++) // mandar para os internos
    {
        if (my_node->internos[i].fd != 0 && my_node->internos[i].id != my_node->internos[recv_from].id)
        {
            if (write(my_node->internos[i].fd, msg, strlen(msg)) == -1)
            {
                printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
                return;
            }
        }
    }
}

void read_msg(Node *tempNode, My_Node *my_node, char *buffer) // Vai ler a mensagem recebida pelo nó
{
    Node extern_Node;

    char name[100];
    int dest = 0, orig = 0, id_leaving = -1;
    memset(name, '\0', sizeof(name));
    memset(&extern_Node, 0, sizeof(extern_Node));

    if (sscanf(buffer, "NEW %d %s %s", &tempNode->id, tempNode->ip, tempNode->port))
    {
        printf("Mensagem recebida: %s\n", buffer);
        memset(buffer, 0, strlen(buffer));
        if (my_node->externo.id == my_node->myinfo.id) // se o meu externo for eu próprio -> tou sozinho -> digo ao novo nó que sou o externo dele & ele é o meu externo
        {
            my_node->externo = *tempNode;
            my_node->tabela[my_node->externo.id] = my_node->externo.id;

            sprintf(buffer, "EXTERN %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
            printf("Mensagem enviada: %s\n", buffer);

            if (write(tempNode->fd, buffer, strlen(buffer)) == -1)
            {
                printf("error: %s\n", strerror(errno));
                exit(1);
            } // envio o meu externo para novo o nó colocar como o seu backup
        }
        else // se o meu externo não for eu próprio -> já tenho externo -> informo o novo nó o meu externo (o backup dele) e guardo-o como interno
        {
            my_node->internos[tempNode->id] = *tempNode;
            my_node->tabela[my_node->internos[tempNode->id].id] = my_node->internos[tempNode->id].id;

            sprintf(buffer, "EXTERN %02d %s %s\n", my_node->externo.id, my_node->externo.ip, my_node->externo.port); // envio o meu externo para novo o nó colocar como o seu backup
            printf("Mensagem envida: %s\n", buffer);
            if (write(tempNode->fd, buffer, strlen(buffer)) == -1)
            {
                printf("error: %s\n", strerror(errno));
                exit(1);
            }
        }
        memset(buffer, 0, strlen(buffer));

    } // registro o novo nó nos meus dados
    else if (sscanf(buffer, "EXTERN %d %s %s", &extern_Node.id, extern_Node.ip, extern_Node.port))
    {
        printf("Mensagem recebida: %s\n", buffer);
        if (tempNode->id == extern_Node.id) // se o id do nó que me envia o externo for igual ao id me enviado entao o meu externo será o no que comunicou comigo
        {
            my_node->externo = *tempNode; // o meu externo é aquele que me enviou a mensagem
            my_node->tabela[my_node->externo.id] = my_node->externo.id;
            my_node->backup = my_node->myinfo;
            // printf("My extern is: %02d %s %s\n", tempNode->id, tempNode->ip, tempNode->port);
            memset(buffer, 0, strlen(buffer));
        }
        else
        {
            my_node->externo = *tempNode;
            my_node->tabela[my_node->externo.id] = my_node->externo.id;
            my_node->backup = extern_Node;
            if (my_node->backup.id != my_node->myinfo.id)
                my_node->tabela[my_node->backup.id] = my_node->externo.id;

            // printf("My extern is: %02d %s %s\n", tempNode->id, tempNode->ip, tempNode->port);
            // printf("My backup is: %02d %s %s\n", extern_Node.id, extern_Node.ip, extern_Node.port);
            memset(buffer, 0, strlen(buffer));
        }
    }
    else if (sscanf(buffer, "QUERY %d %d %s", &dest, &orig, name)) // ainda falta atualizar a tabela
    {
        int found_name = 0, vizinho = 0;
        char msg[128];
        memset(msg, '\0', sizeof(msg));
        my_node->tabela[orig] = tempNode->id; // sei que para ir para o orig posso ir através do nó que me enviou a mensagem

        if (my_node->myinfo.id == dest) // Sou o destino
        {
            vizinho = my_node->tabela[orig];
            for (int i = 0; i < MAX_NAMES; i++)
            {
                if (strcmp(my_node->list[i], name) == 0)
                {
                    sprintf(msg, "CONTENT %d %d %s\n", orig, dest, name);
                    printf("I have what you're looking for: %s\n", name);
                    write_to_nb(my_node, vizinho, msg);
                    found_name = 1;
                    break;
                }
            }
            if (found_name == 0)
            {
                printf("I dont't have what you're looking for: %s, sorry:/ )", name);
                sprintf(msg, "NOCONTENT %d %d %s\n", orig, dest, name);
                write_to_nb(my_node, vizinho, msg);
            }
            return;
        }
        else // Não sou o destino
        {
            // sprintf(msg, "QUERY %02d %02d %s", dest, orig, name); //
            if (my_node->tabela[dest] != -1) // Se o destino já tiver na tabela
            {
                vizinho = my_node->tabela[dest]; // saber qual o vizinho que chega ao destino
                write_to_nb(my_node, vizinho, buffer);
            }
            else // destino não está na tabela tenho de enviar a todos os meus vizinhos Query
            {
                write_to_all(my_node, buffer, tempNode->id);
            }
        }
    }
    else if (sscanf(buffer, "CONTENT %d %d %s", &dest, &orig, name))
    {
        char msg[258];
        memset(msg, '\0', sizeof(msg));

        my_node->tabela[orig] = tempNode->id;

        if (my_node->myinfo.id == dest) // se eu for eu que tiver requisitado o conteudo
        {
            printf("--> Content \"%s\" has been found at node %02d\n", name, orig);
            return;
        }
        else // se não passa a mensagem
        {
            strcpy(msg, buffer);

            if (my_node->tabela[dest] != -1) // Se o destino já tiver na tabela
            {
                int vizinho = my_node->tabela[dest]; // saber qual o vizinho que chega ao destino
                write_to_nb(my_node, vizinho, msg);
            }
            else // destino não está na tabela tenho de enviar a todos os meus vizinhos Query
            {
                write_to_all(my_node, msg, my_node->myinfo.id);
            }
        }
    }
    else if (sscanf(buffer, "NOCONTENT %d %d %s", &dest, &orig, name))
    {
        char msg[258];
        memset(msg, '\0', sizeof(msg));

        my_node->tabela[orig] = tempNode->id;

        if (my_node->myinfo.id == dest) // se eu for eu que tiver requisitado o conteudo
        {
            printf("--> The node %02d don't have the content \"%s\"you asked for\n", orig, name);
            return;
        }
        else // se não passa a mensagem
        {
            strcpy(msg, buffer);

            if (my_node->tabela[dest] != -1) // Se o destino já tiver na tabela
            {
                int vizinho = my_node->tabela[dest]; // saber qual o vizinho que chega ao destino
                write_to_nb(my_node, vizinho, msg);
            }
            else // destino não está na tabela tenho de enviar a todos os meus vizinhos Query
            {
                write_to_all(my_node, msg, my_node->myinfo.id);
            }
        }
    }
    else if (sscanf(buffer, "WITHDRAW %d", &id_leaving))
    {
        my_node->tabela[id_leaving] = -1;
        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (my_node->tabela[i] == id_leaving)
            {
                my_node->tabela[i] = -1;
            }
        }
        write_to_all(my_node, buffer, tempNode->id);
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

    memset(node_list, '\0', sizeof(node_list));
    memset(rdm_node, '\0', sizeof(rdm_node));
    memset(registo, '\0', sizeof(registo));
    memset(msg.recv, '\0', sizeof(msg.recv));
    memset(msg.send, '\0', sizeof(msg.send));
    memset(new_tcp.recv, '\0', sizeof(new_tcp.recv));
    memset(new_tcp.send, '\0', sizeof(new_tcp.send));

    sprintf(msg.send, "NODES %03d", my_node->net);
    strcpy(msg.recv, udp_communication(regIP, regUDP, msg.send)); // udp_communication2(regIP, regUDP, msg) msg.send e msg.recv

    printf("%s\n", msg.recv);

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
        strcpy(rdm_node, node_list[index]);

        sscanf(rdm_node, "%d %s %s", &tempNode->id, tempNode->ip, tempNode->port);

        sprintf(new_tcp.send, "NEW %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
        printf("Mensagem enviada: %s -----> id %02d\n", new_tcp.send, tempNode->id);
        tempNode->fd = tcp_communication(tempNode->ip, tempNode->port, new_tcp.send);
        if (tempNode->fd == 0)
        {
            return;
        }
    }

    sprintf(registo, "REG %03d %02d %s %s", my_node->net, my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
    printf("%s\n", registo);
    ok_reg = udp_communication(regIP, regUDP, registo);
    printf("%s\n", ok_reg);
}

void djoin(My_Node *my_node, Node *tempNode)
{
    char msg_send[328];

    memset(msg_send, 0, sizeof(msg_send));

    sprintf(msg_send, "NEW %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
    printf("Mensagem enviada: %s\n", msg_send);
    tempNode->fd = tcp_communication(tempNode->ip, tempNode->port, msg_send);
    if (tempNode->fd == 0)
    {
        printf("The node you tried to connect doesn't exist");
        return;
    }
}

void leave(Node *leaving_node, My_Node *my_node, Node *tempNode)
{
    char msg_send[128], msg_withdraw[128];
    int i, first_intern = 0, interno = 0;
    memset(msg_send, '\0', sizeof(msg_send));
    memset(msg_withdraw, '\0', sizeof(msg_withdraw));
    sprintf(msg_withdraw, "WITHDRAW %02d\n", leaving_node->id);

    my_node->tabela[leaving_node->id] = -1;
    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (my_node->tabela[i] == leaving_node->id)
        {
            my_node->tabela[i] = -1;
        }
    }

    if (my_node->externo.id == leaving_node->id) // foi um externo que saiu
    {
        for (int i = 0; i < MAX_CONNECTIONS; i++) // mandar para os internos
        {
            if (my_node->internos[i].fd != 0)
            {
                if (write(my_node->internos[i].fd, msg_withdraw, strlen(msg_withdraw)) == -1)
                {
                    printf("Something is wrong with your connections, please try leaving and try to reconnect.\n");
                    return;
                }
            }
        }

        if (my_node->myinfo.id == my_node->backup.id) // o que saiu era uma ancora
        {
            for (int i = 0; i < MAX_CONNECTIONS; i++)
            {
                if (my_node->internos[i].fd != 0)
                {
                    if (first_intern == 0)
                    {
                        my_node->externo = my_node->internos[i];
                        sprintf(msg_send, "EXTERN %02d %s %s\n", my_node->externo.id, my_node->externo.ip, my_node->externo.port);
                        if (write(my_node->externo.fd, msg_send, strlen(msg_send)) == -1)
                        {
                            printf("error: %s\n", strerror(errno));
                            exit(1);
                        }
                        memset(&my_node->internos[i], 0, sizeof(my_node->internos[i]));
                        first_intern = 1;
                    }
                    else
                    {
                        if (write(my_node->internos[i].fd, msg_send, strlen(msg_send)) == -1)
                        {
                            printf("error: %s\n", strerror(errno));
                            exit(1);
                        }
                    }
                    interno = 1;
                }
            }
            if (interno == 0)
            {
                my_node->externo = my_node->myinfo;
            }
        }
        else // o que saiu foi um externo normal
        {
            sprintf(msg_send, "NEW %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
            printf("Mensagem enviada: %s -----> id %02d\n", msg_send, my_node->backup.id);
            tempNode->fd = tcp_communication(my_node->backup.ip, my_node->backup.port, msg_send);
            tempNode->id = my_node->backup.id;
            strcpy(tempNode->ip, my_node->backup.ip);
            strcpy(tempNode->port, my_node->backup.port);

            memset(msg_send, 0, sizeof(msg_send));
            sprintf(msg_send, "EXTERN %02d %s %s\n", my_node->backup.id, my_node->backup.ip, my_node->backup.port); // mando info do meu novo externo aos internos
            // penso que isto aqui em baixo seja desnecessario
            my_node->externo = my_node->backup;
            my_node->backup = my_node->myinfo;
            ///////////////////////////////////////////////////

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
    }
    else // foi um interno que saiu
    {
        memset(&my_node->internos[leaving_node->id], 0, sizeof(my_node->internos[leaving_node->id]));
        for (int i = 0; i < 100; i++) // mandar para os internos
        {
            if (my_node->internos[i].fd != 0)
            {
                if (write(my_node->internos[i].fd, msg_withdraw, strlen(msg_withdraw)) == -1)
                {
                    printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
                    return;
                }
            }
        }

        if (write(my_node->externo.fd, msg_withdraw, strlen(msg_withdraw)) == -1) // mandar para o externo
        {
            printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
            return;
        }
    }
}

void get_name(My_Node *my_node, int dest, char name[100])
{
    char msg[128];
    int vizinho = 0, found_name = 0;
    memset(msg, '\0', sizeof(msg));

    if (my_node->myinfo.id == dest)
    {
        for (int i = 0; i < MAX_NAMES; i++)
        {
            if (strcmp(my_node->list[i], name) == 0)
            {
                printf("I have what you're looking for: %s\n", name);
                found_name = 1;
                break;
            }
        }
        if (found_name == 0)
        {
            printf("I dont't have what you're looking for: %s, sorry:/ )\n", name);
        }
        return;
    }
    else
    {
        sprintf(msg, "QUERY %02d %02d %s\n", dest, my_node->myinfo.id, name);
        // printf("%s", msg);
        if (my_node->tabela[dest] != -1) // Se o destino já tiver na tabela
        {
            vizinho = my_node->tabela[dest]; // saber qual o vizinho que chega ao destino
            write_to_nb(my_node, vizinho, msg);
        }
        else // destino não está na tabela tenho de enviar a todos os meus vizinhos Query
        {
            write_to_all(my_node, msg, my_node->myinfo.id); // falta nao mandar ao que me m
        }
    }
}

void commands(char *input, char *reg_ip, char *reg_udp, My_Node *my_node, Node *tempNode)
{
    char name[100];
    int dest;
    memset(name, '\0', sizeof(name));
    if (my_node->net == -1)
    {
        if (sscanf(input, "join %d %d\n", &my_node->net, &my_node->myinfo.id))
        {
            if (my_node->myinfo.id >= 0 && my_node->myinfo.id <= 99 && my_node->net >= 0 && my_node->net <= 999)
            {
                my_node->externo = my_node->myinfo;
                my_node->backup = my_node->myinfo;
                join(reg_ip, reg_udp, my_node, tempNode);
            }
            else
            {
                my_node->net = -1;
                my_node->myinfo.id = -1;
                printf("Rede ou id não fazem sentido\n");
            }
        }

        else if (sscanf(input, "djoin %d %d %d %s %s", &my_node->net, &my_node->myinfo.id, &tempNode->id, tempNode->ip, tempNode->port))
        {
            if (my_node->myinfo.id >= 0 && my_node->myinfo.id <= 99 && my_node->net >= 0 && my_node->net <= 999)
            {
                if (my_node->myinfo.id == tempNode->id && (strcmp(my_node->myinfo.port, tempNode->port) == 0))
                {
                    my_node->externo = my_node->myinfo;
                    my_node->backup = my_node->myinfo;
                    printf("Sou o primeiro nó\n");
                }
                else if (my_node->myinfo.id != tempNode->id && (strcmp(my_node->myinfo.port, tempNode->port) != 0))
                {
                    printf("port_: %s\n", my_node->myinfo.port);
                    my_node->externo = my_node->myinfo;
                    my_node->backup = my_node->myinfo;
                    djoin(my_node, tempNode);
                }
                else
                {
                    printf("Try again\n");
                }
            }
            else
            {
                printf("Rede ou id não fazem sentido\n");
            }
        }
    }

    if (sscanf(input, "create %s", name)) // create name
    {
        for (int i = 0; i < MAX_NAMES; i++)
        {
            if (my_node->list[i][0] == '\0')
            {
                strcpy(my_node->list[i], name);            // copy the item to the list
                my_node->list[i][strlen(name) + 1] = '\0'; // make sure the string is null-terminated
                break;
            }
        }
        printf("New name created successfully: %s\n", name);
    }
    else if (sscanf(input, "delete %s", name)) // delete name
    {
        int delete = 0;

        for (int i = 0; i < MAX_NAMES; i++)
        {
            if (strcmp(my_node->list[i], name) == 0)
            {
                memset(my_node->list[i], '\0', sizeof(my_node->list[i]));
                delete = 1;
                break;
            }
        }
        if (delete == 1)
        {
            printf("File deleted successfully.\n");
        }
        else
        {
            printf("There is no %s in this list\n", name);
        }
    }
    else if (sscanf(input, "get %d %s", &dest, name)) // get name
    {
        get_name(my_node, dest, name);
    }
    else if (strcmp(input, "st\n") == 0)
    {
        printf("===== SHOW TOPOLOGY =====\n");
        printf("My Node: %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
        if (my_node->externo.id == -1)
        {
            printf("Externo:\n");
        }
        else
        {
            printf("Externo: %02d %s %s\n", my_node->externo.id, my_node->externo.ip, my_node->externo.port);
        }
        if (my_node->backup.id == -1)
        {
            printf("Backup:\n");
        }
        else
        {
            printf("Backup: %02d %s %s\n", my_node->backup.id, my_node->backup.ip, my_node->backup.port);
        }
        printf("Internos: \n");

        for (int i = 0; i <= 99; i++)
        {
            if (my_node->internos[i].fd != 0)
            {
                printf("%02d %s %s\n", my_node->internos[i].id, my_node->internos[i].ip, my_node->internos[i].port);
            }
        }

        printf("=========================\n");
        return;
    }
    else if (strcmp(input, "sn\n") == 0)
    {
        printf("===== SHOW NAMES =====\n");
        printf("My files are: \n");
        for (int j = 0; j <= 99; j++)
            if (my_node->list[j][0] != '\0')
                printf("\t%s\n", my_node->list[j]);

        printf("======================\n");
        return;
    }
    else if (strcmp(input, "sr\n") == 0)
    {
        printf("===== SHOW ROUTING =====\n");
        printf("My route table is: \n");
        printf("\tdestino\tvizinho\n");
        for (int j = 0; j <= 99; j++)
            if (my_node->tabela[j] != -1)
                printf("\t%d\t%d\n\n", j, my_node->tabela[j]);

        printf("========================\n");
        return;
    }
    else if (strcmp(input, "leave\n") == 0)
    {
        char registo[128], ok_reg[128];
        char *line;

        memset(registo, '\0', sizeof(registo));
        memset(ok_reg, '\0', sizeof(ok_reg));

        if (my_node->net == -1)
        {
            printf("=\nYou are not in a network\n=\n");
            return;
        }

        sprintf(registo, "NODES %03d", my_node->net);
        strcpy(ok_reg, udp_communication(reg_ip, reg_udp, registo));

        line = strtok(ok_reg, "\n");
        line = strtok(NULL, "\n");
        if (line != NULL) // Se for no djoin não faz sentido dar unreg
        {
            memset(registo, '\0', sizeof(registo));
            memset(ok_reg, '\0', sizeof(ok_reg));
            sprintf(registo, "UNREG %03d %02d", my_node->net, my_node->myinfo.id); // tudo muito giro mas nao funciona se for por djoin
            printf("%s\n", registo);
            strcpy(ok_reg, udp_communication(reg_ip, reg_udp, registo));
            printf("%s\n", ok_reg);
        }

        memset(&my_node->internos, 0, sizeof(my_node->internos));
        memset(&my_node->externo, 0, sizeof(my_node->externo)); // limpar externo
        memset(my_node->tabela, -1, sizeof(my_node->tabela));

        my_node->myinfo.id = -1;
        my_node->net = -1;
        my_node->externo = my_node->myinfo; // volto a meter na forma default
        my_node->backup = my_node->myinfo;
    }
    else if (strcmp(input, "exit\n") == 0)
    {
        char registo[128], ok_reg[128];

        memset(registo, '\0', sizeof(registo));
        memset(ok_reg, '\0', sizeof(ok_reg));
        if (my_node->net == -1)
        {
            exit(1);
        }
        else
        {
            memset(registo, '\0', sizeof(registo));
            memset(ok_reg, '\0', sizeof(ok_reg));
            sprintf(registo, "UNREG %03d %02d", my_node->net, my_node->myinfo.id); // tudo muito giro mas nao funciona se for por djoin
            printf("%s\n", registo);
            strcpy(ok_reg, udp_communication(reg_ip, reg_udp, registo));
            printf("%s\n", ok_reg);
            exit(1);
        }
    }
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
    char buffer[MAX_BUFFER_SIZE], msg[128];
    int newfd, n, command_len = 0;
    int client_count = 0, maxfd = 0, counter, i, j;
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
    memset(buffer, '\0', sizeof(buffer));
    memset(&my_node.internos, 0, sizeof(my_node.internos));
    memset(&my_node.tabela, -1, sizeof(my_node.tabela));
    memset(&my_node.list, '\0', sizeof(my_node.list));
    memset(msg, '\0', sizeof(msg));

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

        printf("Welcome to the server please select your next command from this list:\n -join net id\n -djoin net id bootid bootIP bootTCP\n"
               " -create name\n -delete name\n -get dest name\n -show topology (st)\n -show names (sn)\n -show routing (sr)\n -leave\n -exit\n");
    }
    strcpy(reg_ip, argv[3]);
    strcpy(reg_udp, argv[4]);

    // setup da info do meu nó e abertura do TCP Server
    strcpy(my_node.myinfo.ip, argv[1]);
    strcpy(my_node.myinfo.port, argv[2]);

    my_node.myinfo.fd = tcp_listener(my_node.myinfo.port); // abertura de uma porta tcp para listen
    my_node.myinfo.id = -1;
    my_node.net = -1;
    my_node.externo = my_node.myinfo;
    my_node.backup = my_node.myinfo;
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

        if (counter <= 0)
        { /*error*/
            exit(1);
        }

        while (counter > 0)
        {

            if (FD_ISSET(my_node.myinfo.fd, &rfds)) // Notificacao no meu server TCP // o unico caso que isto acontece é a receber uma msg "NEW IP TCP"
            {
                FD_CLR(my_node.myinfo.fd, &rfds);
                addrlen = sizeof(addr);
                if ((newfd = accept(my_node.myinfo.fd, (struct sockaddr *)&addr, &addrlen)) == -1)
                { /*error*/
                    printf("Error to connecting to node");
                    counter--;
                    break;
                }

                clients[client_count].fd = newfd;
                if (maxfd < newfd)
                {
                    maxfd = newfd;
                }
                client_count++;
                counter--;
            }
            else if (FD_ISSET(STDIN_FILENO, &rfds)) // Notificao no user input
            {
                FD_CLR(STDIN_FILENO, &rfds);
                memset(buffer, '\0', sizeof(buffer));

                if ((n = read(STDIN_FILENO, buffer, 128 + 1)) < 0)
                    exit(1);
                else
                {
                    buffer[n] = '\0';
                    if (strcmp(buffer, "leave\n") == 0 || strcmp(buffer, "exit\n") == 0)
                    {
                        for (i = 0; i < client_count; i++)
                        {
                            close(clients[i].fd);
                        }
                        client_count = 0;
                        memset(&clients, 0, sizeof(clients)); // Se der poop apaga
                    }
                    commands(buffer, reg_ip, reg_udp, &my_node, &tempNode);

                    if (tempNode.fd != 0)
                    {
                        clients[client_count++] = tempNode;
                        for (i = 0; i < client_count; i++)
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
            else
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
                                printf("Problem in reading the message\n");
                                break;
                            }
                            if (n == 0)
                            {
                                // funcao leave recebe como argumentos o nó que sai, a info do meu nó e o possível nó que terei de me juntar
                                close(clients[i].fd); // garanto que a conexão do nó que saiu está fechada
                                leave(&(clients[i]), &my_node, &tempNode);
                                memset(&clients[i], 0, sizeof(clients[i])); // limpo toda a informação guardada pelo nó no array
                                for (j = (client_count - 1); j >= 0; j--)   // organização da lista de nós
                                {
                                    if (clients[(client_count - 1)].fd == 0) // Se no index do client_count dos clients tiver a 0, foi esse nó que saiu não é preciso mais nada
                                    {

                                        break;
                                    }
                                    else // Se o nó que saiu tiver a meio do array, meter o último nó do array e meter no lugar do que saiu
                                    {
                                        if (clients[j].fd == 0)
                                        {
                                            clients[j] = clients[(client_count - 1)];
                                            memset(&clients[(client_count - 1)], 0, sizeof(clients[(client_count - 1)])); // limpar o lugar do último nó que foi reorganizado
                                        }
                                    }
                                }
                                client_count--;
                                if (tempNode.fd != 0) // Aqueles a que se ligaram ao backup precisam de adicionar um novo nó a lista
                                {
                                    clients[client_count++] = tempNode;
                                    for (int i = 0; i < client_count; i++)
                                    {
                                        if (clients[i].fd > maxfd)
                                        {
                                            maxfd = clients[i].fd;
                                        }
                                    }
                                }
                                maxfd = my_node.myinfo.fd; // procuramos o novo maxfd
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
                                for (int j = 0; j < n; j++)
                                {
                                    char c = buffer[j];
                                    if (c == '\n')
                                    {
                                        clients[i].buffer[command_len] = '\0';
                                        read_msg(&(clients[i]), &my_node, clients[i].buffer);
                                        memset(clients[i].buffer, '\0', sizeof(clients[i].buffer));
                                        command_len = 0;
                                    }
                                    else
                                    {
                                        clients[i].buffer[command_len] = c;
                                        command_len++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
