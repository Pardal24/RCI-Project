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
    char buffer[128]; // necessário para ler mensagens que não cheguem completas
    int fd;
} Node;

typedef struct My_Node
{
    Node myinfo;
    Node internos[MAX_CONNECTIONS];
    Node externo;
    Node backup;
    int tabela[MAX_CONNECTIONS];        // index é o dest o conteudo o vizinho
    char list[MAX_NAMES][MAX_NAME_LEN]; // lista de nomes
    int net;
} My_Node;

char *My_IP() // Função para obter o meu IP
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

char *udp_communication(char *IP_server, char *port, char *msg) // Envio e receção de mensagens UDP para o servidor
{
    struct addrinfo hints, *res;
    int fd, errcode;
    ssize_t n;
    char *msg_rcv = NULL;
    fd_set rfds;
    struct timeval tv;

    fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    if (fd == -1)                        /*error*/
        return NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    errcode = getaddrinfo(IP_server, port, &hints, &res);
    if (errcode != 0)
    { /*error*/
        close(fd);
        return NULL;
    }
    n = sendto(fd, msg, strlen(msg), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1)
    { /*error*/
        freeaddrinfo(res);
        close(fd);
        return NULL;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int retval = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (retval == -1)
    {
        /* error */
        freeaddrinfo(res);
        close(fd);
        return NULL;
    }
    else if (retval == 0)
    {
        /* timeout */
        freeaddrinfo(res);
        close(fd);
        return NULL;
    }

    struct sockaddr addr;
    socklen_t addrlen;
    char buffer[2016];
    addrlen = sizeof(addr);

    n = recvfrom(fd, buffer, 2016, 0, &addr, &addrlen);
    if (n == -1) /*error*/
        return NULL;
    buffer[n] = '\0';
    msg_rcv = buffer;
    close(fd);
    return msg_rcv;
}

int tcp_communication(char *n_ip, char *n_port, char *msg) // Setup de uma nova ligação TCP e envio de mensagem
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

void write_to_nb(My_Node *my_node, int vizinho, char msg[128]) // Mandar/reencaminhar mensagem para o vizinho da tabela de expedição
{
    // como a função apenas sabe o id do vizinho, tem de verificar se é externo ou interno para aceder ao fd correto

    if (my_node->externo.id == vizinho) // Verificar se o vizinho em questão é o meu externo
    {
        if (write(my_node->externo.fd, msg, strlen(msg)) == -1)
        {
            printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
            return;
        }
    }
    else // Se não for o externo, então é um interno
    {
        if (write(my_node->internos[vizinho].fd, msg, strlen(msg)) == -1)
        {
            printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
            return;
        }
    }
}

void write_to_all(My_Node *my_node, char msg[128], int recv_from) // Mandar/reencaminhar mensagem para todos os vizinhos exceto o que enviou a mensagem
{

    if (my_node->externo.id != recv_from) // Verificar se o externo não é o que enviou a mensagem
    {
        if (write(my_node->externo.fd, msg, strlen(msg)) == -1) // Mandar para o externo
        {
            printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
            return;
        }
    }
    for (int i = 0; i < MAX_CONNECTIONS; i++) // Mandar para os internos
    {
        if (my_node->internos[i].fd != 0 && my_node->internos[i].id != my_node->internos[recv_from].id) // Verificar se o fd é válido e se o id do nó é diferente do que enviou a mensagem
        {
            if (write(my_node->internos[i].fd, msg, strlen(msg)) == -1)
            {
                printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
                return;
            }
        }
    }
}

void read_msg(Node *tempNode, My_Node *my_node, char *buffer) // Função que lê e trata as mensagens recebidas por outros nós
{
    Node extern_Node;

    char name[100];
    int dest = 0, orig = 0, id_leaving = -1;
    memset(name, '\0', sizeof(name));
    memset(&extern_Node, 0, sizeof(extern_Node));

    if (sscanf(buffer, "NEW %d %s %s", &tempNode->id, tempNode->ip, tempNode->port)) // NEW
    {
        printf("Mensagem recebida: %s\n", buffer);
        memset(buffer, 0, strlen(buffer));
        if (my_node->externo.id == my_node->myinfo.id) // se o meu externo for eu próprio => tou sozinho => batizar o novo nó como âncora
        {
            my_node->externo = *tempNode;
            my_node->tabela[my_node->externo.id] = my_node->externo.id;

            sprintf(buffer, "EXTERN %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
            printf("Mensagem enviada: %s\n", buffer);

            if (write(tempNode->fd, buffer, strlen(buffer)) == -1)
            {
                printf("error: %s\n", strerror(errno));
                exit(1);
            }
        }
        else // se o meu externo não for eu próprio => já tenho externo => informo o novo nó o meu externo (o backup dele) e guardo-o como interno
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

    }                                                                                              // registro o novo nó nos meus dados
    else if (sscanf(buffer, "EXTERN %d %s %s", &extern_Node.id, extern_Node.ip, extern_Node.port)) // EXTERN
    {
        printf("Mensagem recebida: %s\n", buffer);
        if (tempNode->id == extern_Node.id) // Se recebi um id igual ao id de quem me enviou msg, então fui batizado como âncora
        {
            my_node->externo = *tempNode;
            my_node->tabela[my_node->externo.id] = my_node->externo.id; // add à tabela de expedição
            my_node->backup = my_node->myinfo;
            memset(buffer, 0, strlen(buffer));
        }
        else // Caso contrário, quem me mandou mensagem é o meu externo e guardo a info do meu backup
        {
            my_node->externo = *tempNode;
            my_node->tabela[my_node->externo.id] = my_node->externo.id; // add à tabela de expedição
            my_node->backup = extern_Node;
            if (my_node->backup.id != my_node->myinfo.id)
                my_node->tabela[my_node->backup.id] = my_node->externo.id;

            memset(buffer, 0, strlen(buffer));
        }
    }
    else if (sscanf(buffer, "QUERY %d %d %s", &dest, &orig, name)) // QUERY
    {
        int found_name = 0, vizinho = 0;
        char msg[128];
        memset(msg, '\0', sizeof(msg));
        my_node->tabela[orig] = tempNode->id; // sei que para ir para o orig posso ir através do nó que me enviou a mensagem => adiciona à tabela de expedição

        if (my_node->myinfo.id == dest) // Se eu for o destino, respondo à mensagem
        {
            vizinho = my_node->tabela[orig];
            for (int i = 0; i < MAX_NAMES; i++)
            {
                if (strcmp(my_node->list[i], name) == 0) // procura pelo ficheiro na lista de ficheiros
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
        else // Não sou o destino, reencaminho a mensagem
        {
            if (my_node->tabela[dest] != -1) // Se o destino já tiver na tabela, mando para o vizinho que chega ao destino
            {
                vizinho = my_node->tabela[dest];
                write_to_nb(my_node, vizinho, buffer);
            }
            else // destino não está na tabela tenho de enviar a todos os meus vizinhos
            {
                write_to_all(my_node, buffer, tempNode->id);
            }
        }
    }
    else if (sscanf(buffer, "CONTENT %d %d %s", &dest, &orig, name))
    {
        char msg[258];
        memset(msg, '\0', sizeof(msg));

        my_node->tabela[orig] = tempNode->id; // sei que para ir para o orig posso ir através do nó que me enviou a mensagem => adiciona à tabela de expedição

        if (my_node->myinfo.id == dest) // Se eu for eu que tiver requisitado o conteudo
        {
            printf("--> Content \"%s\" has been found at node %02d\n", name, orig);
            return;
        }
        else // Caso contrário, reencaminho a mensagem
        {
            strcpy(msg, buffer);

            if (my_node->tabela[dest] != -1) // Se o destino já tiver na tabela, mando para o vizinho que chega ao destino
            {
                int vizinho = my_node->tabela[dest];
                write_to_nb(my_node, vizinho, msg);
            }
            else // destino não está na tabela tenho de enviar a todos os meus vizinhos
            {
                write_to_all(my_node, msg, my_node->myinfo.id);
            }
        }
    }
    else if (sscanf(buffer, "NOCONTENT %d %d %s", &dest, &orig, name))
    {
        char msg[258];
        memset(msg, '\0', sizeof(msg));

        my_node->tabela[orig] = tempNode->id; // sei que para ir para o orig posso ir através do nó que me enviou a mensagem => adiciona à tabela de expedição

        if (my_node->myinfo.id == dest) // Se eu for eu que tiver requisitado o conteudo
        {
            printf("--> The node %02d don't have the content \"%s\"you asked for\n", orig, name);
            return;
        }
        else // Caso contrário, reencaminho a mensagem
        {
            strcpy(msg, buffer);

            if (my_node->tabela[dest] != -1) // Se o destino já tiver na tabela, mando para o vizinho que chega ao destino
            {
                int vizinho = my_node->tabela[dest];
                write_to_nb(my_node, vizinho, msg);
            }
            else // destino não está na tabela tenho de enviar a todos os meus vizinhos
            {
                write_to_all(my_node, msg, my_node->myinfo.id);
            }
        }
    }
    else if (sscanf(buffer, "WITHDRAW %d", &id_leaving))
    {
        my_node->tabela[id_leaving] = -1; // retira o nó como destino que saiu da tabela de expedição (set a -1)

        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (my_node->tabela[i] == id_leaving)
            {
                my_node->tabela[i] = -1; // retira todas as entradas da tabela que requerem enviar para o nó que saiu
            }
        }
        write_to_all(my_node, buffer, tempNode->id); // reencaminha a mensagem para todos os meus vizinhos, menos para o nó que enviou a mensagem
    }
}

int cmpr_id(char *node_list[], My_Node *my_node, int size) // Função auxiliar para verificar se o id do nó já existe na rede
{

    int i, confirm = 0, j = 0, id;
    int id_list[100];

    memset(id_list, 0, sizeof(id_list));

    while (j <= size && node_list[j] != NULL) // verifica se já existe um nó com o mesmo id
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
    if (confirm == 1) // se já existir um nó com o mesmo id, procura um id livre
    {
        for (i = 0; i < 100; i++)
        {
            if (id_list[i] == 0)
            {
                my_node->myinfo.id = i;
                return 1; // retorna 1 se houve uma alteração ao id
            }
        }
    }

    return 0; // retorna 0 se não houve alteração ao id
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
        while (line != NULL && i < MAX_CONNECTIONS) // lê a lista de nós e guarda numa array
        {
            node_list[i] = line;
            line = strtok(NULL, "\n");
            i++;
        }
        if (cmpr_id(node_list, my_node, size) == 1) // verifica se o id do nó já existe na rede
        {
            printf("There is already your id, your new id is: %02d\n", my_node->myinfo.id);
            my_node->externo.id = my_node->myinfo.id;
            my_node->backup.id = my_node->myinfo.id;
        }
        else
        {
            printf("Ready to connect\n");
        }

        index = rand() % i;                 // escolhe um nó aleatório da lista para se ligar
        strcpy(rdm_node, node_list[index]); // guarda o nó escolhido numa string

        sscanf(rdm_node, "%d %s %s", &tempNode->id, tempNode->ip, tempNode->port); // guarda o nó escolhido numa struct

        sprintf(new_tcp.send, "NEW %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port); // envia um NEW para o nó escolhido
        printf("Mensagem enviada: %s -----> id %02d\n", new_tcp.send, tempNode->id);
        tempNode->fd = tcp_communication(tempNode->ip, tempNode->port, new_tcp.send);
        if (tempNode->fd == 0)
        {
            return;
        }
    }

    // Por fim, registra-se no servidor de nós

    sprintf(registo, "REG %03d %02d %s %s", my_node->net, my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
    printf("%s\n", registo);
    ok_reg = udp_communication(regIP, regUDP, registo);
    printf("%s\n", ok_reg);
}

void djoin(My_Node *my_node, Node *tempNode) // envia um NEW para o nó escolhido sem se registar no servidor de nós
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

void leave(Node *leaving_node, My_Node *my_node, Node *tempNode) // Função que envia deteta a saída de um nó vizinho e envia um WITHDRAW para todos os nós da rede
{
    char msg_send[128], msg_withdraw[128];
    int i, first_intern = 0, interno = 0;
    memset(msg_send, '\0', sizeof(msg_send));
    memset(msg_withdraw, '\0', sizeof(msg_withdraw));
    sprintf(msg_withdraw, "WITHDRAW %02d\n", leaving_node->id);

    my_node->tabela[leaving_node->id] = -1; // retira o nó da tabela de expedição
    for (i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (my_node->tabela[i] == leaving_node->id)
        {
            my_node->tabela[i] = -1;
        }
    }

    if (my_node->externo.id == leaving_node->id) // Foi um externo que saiu
    {
        for (int i = 0; i < MAX_CONNECTIONS; i++) // Antes de tudo, manda withdraw para todos os internos
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

        if (my_node->myinfo.id == my_node->backup.id) // Se eu sou âncora => o meu externo também era âncora => tenho de escolher nova ancora
        {
            for (int i = 0; i < MAX_CONNECTIONS; i++)
            {
                if (my_node->internos[i].fd != 0)
                {
                    if (first_intern == 0) // Escolhe um interno
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
                    else // Atualiza o backup dos restantes
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
            if (interno == 0) // se não houver internos, estou sozinho e volto ao estado default
            {
                my_node->externo = my_node->myinfo;
            }
        }
        else // o que saiu foi um externo normal, ligo-me ao meu backup
        {
            sprintf(msg_send, "NEW %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
            printf("Mensagem enviada: %s -----> id %02d\n", msg_send, my_node->backup.id);
            tempNode->fd = tcp_communication(my_node->backup.ip, my_node->backup.port, msg_send); // envia mensagem ao backcup
            tempNode->id = my_node->backup.id;
            strcpy(tempNode->ip, my_node->backup.ip);
            strcpy(tempNode->port, my_node->backup.port);

            memset(msg_send, 0, sizeof(msg_send));
            sprintf(msg_send, "EXTERN %02d %s %s\n", my_node->backup.id, my_node->backup.ip, my_node->backup.port);
            my_node->externo = my_node->backup;
            my_node->backup = my_node->myinfo;

            for (int i = 0; i < 100; i++) // mando info do meu novo externo aos meus internos
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
        memset(&my_node->internos[leaving_node->id], 0, sizeof(my_node->internos[leaving_node->id])); // retira o nó da lista de internos

        for (int i = 0; i < 100; i++) // mandar WITHDRAW para todos os internos
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

        if (write(my_node->externo.fd, msg_withdraw, strlen(msg_withdraw)) == -1) // mandar WITHDRAW para o externo
        {
            printf("Something is wrong with your connections, please try giving leave and try to reconnect.\n");
            return;
        }
    }
}

void get_name(My_Node *my_node, int dest, char name[100]) // Função que lê um GET e envia QUERY para os vizinhos
{
    char msg[128];
    int vizinho = 0, found_name = 0;
    memset(msg, '\0', sizeof(msg));

    if (my_node->myinfo.id == dest) // Se o destino for o meu id
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
    else // Cria a mensagem e envia para os vizinhos
    {
        sprintf(msg, "QUERY %02d %02d %s\n", dest, my_node->myinfo.id, name);

        if (my_node->tabela[dest] != -1) // Se o destino já tiver na tabela, mando para o vizinho correspondente
        {
            vizinho = my_node->tabela[dest];
            write_to_nb(my_node, vizinho, msg);
        }
        else // destino não está na tabela tenho de enviar a todos os meus vizinhos
        {
            write_to_all(my_node, msg, my_node->myinfo.id);
        }
    }
}

void commands(char *input, char *reg_ip, char *reg_udp, My_Node *my_node, Node *tempNode) // Função que lê os comandos do utilizador
{
    char name[100];
    int dest;
    memset(name, '\0', sizeof(name));
    if (my_node->net == -1) // Só posso dar join/djoin se não estiver ligado a nenhuma rede
    {
        if (sscanf(input, "join %d %d\n", &my_node->net, &my_node->myinfo.id))
        {
            if (my_node->myinfo.id >= 0 && my_node->myinfo.id <= 99 && my_node->net >= 0 && my_node->net <= 999) // verifica se o id e a rede estão dentro dos limites
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
            if (my_node->myinfo.id >= 0 && my_node->myinfo.id <= 99 && my_node->net >= 0 && my_node->net <= 999) // verifica se o id e a rede estão dentro dos limites
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

    if (sscanf(input, "create %s", name)) // Cria um nome e adiciona à lista
    {
        for (int i = 0; i < MAX_NAMES; i++)
        {
            if (my_node->list[i][0] == '\0') // procura uma posição vazia na lista
            {
                strcpy(my_node->list[i], name);
                my_node->list[i][strlen(name) + 1] = '\0';
                break;
            }
        }
        printf("New name created successfully: %s\n", name);
    }
    else if (sscanf(input, "delete %s", name)) // Apaga um nome da lista
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
    else if (sscanf(input, "get %d %s", &dest, name)) // Procura um nome na rede
    {
        get_name(my_node, dest, name);
    }
    else if (strcmp(input, "st\n") == 0) // SHOW TOPOLOGY
    {
        printf("===== SHOW TOPOLOGY =====\n");
        printf("My Node: %02d %s %s\n", my_node->myinfo.id, my_node->myinfo.ip, my_node->myinfo.port);
        if (my_node->externo.id == -1)
            printf("Externo:\n");
        else
            printf("Externo: %02d %s %s\n", my_node->externo.id, my_node->externo.ip, my_node->externo.port);

        if (my_node->backup.id == -1)
            printf("Backup:\n");
        else
            printf("Backup: %02d %s %s\n", my_node->backup.id, my_node->backup.ip, my_node->backup.port);

        printf("Internos: \n");
        for (int i = 0; i <= 99; i++)
            if (my_node->internos[i].fd != 0)
                printf("%02d %s %s\n", my_node->internos[i].id, my_node->internos[i].ip, my_node->internos[i].port);

        printf("=========================\n");
        return;
    }
    else if (strcmp(input, "sn\n") == 0) // SHOW NAMES
    {
        printf("===== SHOW NAMES =====\n");
        printf("My files are: \n");
        for (int j = 0; j <= 99; j++)
            if (my_node->list[j][0] != '\0')
                printf("\t%s\n", my_node->list[j]);

        printf("======================\n");
        return;
    }
    else if (strcmp(input, "sr\n") == 0) // SHOW ROUTING
    {
        printf("===== SHOW ROUTING =====\n");
        printf("My route table is: \n");
        printf("\tdestino\tvizinho\n");
        for (int j = 0; j < MAX_CONNECTIONS; j++)
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

        if (my_node->net == -1) // Se não estiver em nenhuma rede não faz sentido dar leave
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
            sprintf(registo, "UNREG %03d %02d", my_node->net, my_node->myinfo.id); // UNREG do nó
            printf("%s\n", registo);
            strcpy(ok_reg, udp_communication(reg_ip, reg_udp, registo));
            printf("%s\n", ok_reg);
        }

        // Reset às variáveis

        memset(&my_node->internos, 0, sizeof(my_node->internos));
        memset(&my_node->externo, 0, sizeof(my_node->externo));
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
        if (my_node->net == -1) // se já tiver dado leave, só dá exit
        {
            exit(1);
        }
        else // Caso contrário, dá leave e depois exit
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

int tcp_listener(char *port) // Função que cria um socket para o servidor TCP do nó
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
    struct timeval timeout;
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

    if (argc != 5) // Verificar se o utilizador colocou os argumentos todos
    {
        printf("Number of arguments is wrong, you are wrong\n Call the program with: IP TCP regIP(193.136.138.142) regUDP(59000)\n");
        exit(0);
    }
    else
    {
        if (strcmp(argv[1], My_IP()) != 0) // Verificar se o IP é válido
        {
            printf("Your IP is: %s\n", My_IP()); // ver qual o ip a usar
            // exit(0);                             // (exit necessário devido a má invocação)
        }

        printf("Welcome to the server please select your next command from this list:\n -join net id\n -djoin net id bootid bootIP bootTCP\n"
               " -create name\n -delete name\n -get dest name\n -show topology (st)\n -show names (sn)\n -show routing (sr)\n -leave\n -exit\n");
    }

    strcpy(reg_ip, argv[3]);
    strcpy(reg_udp, argv[4]);

    // setup da info do meu nó e abertura do TCP Server
    strcpy(my_node.myinfo.ip, argv[1]);
    strcpy(my_node.myinfo.port, argv[2]);

    my_node.myinfo.fd = tcp_listener(my_node.myinfo.port); // abertura de uma socket para receber conexões TCP
    my_node.myinfo.id = -1;
    my_node.net = -1;
    my_node.externo = my_node.myinfo;
    my_node.backup = my_node.myinfo;
    maxfd = my_node.myinfo.fd;

    while (1)
    {
        memset(&tempNode, 0, sizeof(tempNode)); // Struct temporária auxiliar

        FD_ZERO(&rfds); // clear ao rfds

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

        timeout.tv_sec = 1;
        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);

        if (counter < 0)
        { /*error*/
            exit(1);
        }
        else if (counter == 0) // timeout
        {
            for (i = 0; i < client_count; i++)
            {
                if (clients[i].id == -1 && clients[i].fd != 0) // se demorar muito tempo a enviar o NEW
                {
                    close(clients[i].fd);
                    printf("Closed a connection with one Node because it didn't send NEW in time\n");
                    memset(&clients[i], 0, sizeof(clients[i])); // retiro da lista de clientes

                    for (j = (client_count - 1); j >= 0; j--) // organização da lista de clientes
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
                }
            }
        }
        else
        {

            while (counter > 0)
            {

                if (FD_ISSET(my_node.myinfo.fd, &rfds)) // Notificacao no meu server TCP => cria um novo socket para o cliente e adiciona-o ao array de clientes
                {
                    FD_CLR(my_node.myinfo.fd, &rfds);
                    addrlen = sizeof(addr);
                    if ((newfd = accept(my_node.myinfo.fd, (struct sockaddr *)&addr, &addrlen)) == -1) // aceita a conexão TCP
                    {                                                                                  /*error*/
                        printf("Error to connecting to node");
                        counter--;
                        break;
                    }

                    clients[client_count].fd = newfd; // adiciona o novo cliente ao array de clientes
                    clients[client_count].id = -1;
                    if (maxfd < newfd)
                    {
                        maxfd = newfd;
                    }
                    client_count++;
                    counter--;
                }
                else if (FD_ISSET(STDIN_FILENO, &rfds)) // Notificao no user input => le o comando e executa-o
                {
                    FD_CLR(STDIN_FILENO, &rfds);
                    memset(buffer, '\0', sizeof(buffer));

                    if ((n = read(STDIN_FILENO, buffer, 128 + 1)) < 0) // le o comando do user
                        exit(1);
                    else
                    {
                        buffer[n] = '\0';
                        if (strcmp(buffer, "leave\n") == 0 || strcmp(buffer, "exit\n") == 0) // se o comando for leave ou exit fecha todas as conexões TCP
                        {
                            for (i = 0; i < client_count; i++)
                            {
                                close(clients[i].fd);
                            }
                            client_count = 0;
                            memset(&clients, 0, sizeof(clients));
                        }
                        commands(buffer, reg_ip, reg_udp, &my_node, &tempNode); // executa o comando

                        if (tempNode.fd != 0) // se o comando for join ou djoin adiciona o novo cliente ao array de clientes
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
                else // Notificacao num dos sockets de clientes TCP => le a mensagem e executa-a
                {
                    for (; counter > 0; counter--)
                    {
                        for (i = 0; i < client_count; i++) // Procuro o cliente que enviou a mensagem
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
                                else if (n == 0) // Deteção se o cliente fechou a conexão
                                {
                                    close(clients[i].fd);                      // Fecho a conexão do nó que saiu
                                    leave(&(clients[i]), &my_node, &tempNode); // Retiro o nó que saiu da minha lista de vizinhos e envio a informação aos meus vizinhos

                                    memset(&clients[i], 0, sizeof(clients[i])); // retiro da lista de clientes

                                    for (j = (client_count - 1); j >= 0; j--) // organização da lista de clientes
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
    }
    return 0;
}