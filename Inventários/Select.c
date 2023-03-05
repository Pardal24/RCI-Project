#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
/* ... */
#define max(A, B) ((A) >= (B) ? (A) : (B))
int main(void)
{
    int fd, newfd;
    int *client_fds = NULL;
    fd_set rfds;
    int maxfd, counter;
    /*...*/
    /*fd=socket(…);bind(fd,…);listen(fd,…);*/
    while (1)
    {
        FD_ZERO(&rfds);

        FD_SET(fd_listen, &rfds);
        FD_SET(STDIN_FILENO, &rfds);

        for (i = 0; i < client_count; i++)
        {
            FD_SET(client_fds[i], &readfds);
            if (client_fds[i] > max_fd)
            {
                max_fd = client_fds[i];
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
                    close(afd);
                    counter--;
                }
            }
            else
            {
                for (i = 0; i < client_count; i++)
                {
                    if (FD_ISSET(client_fds[i], &readfds))
                    {
                        int n = read(client_fds[i], buffer, MAX_BUFFER_SIZE);
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
        } // switch(state)
    }     // while(1)
    /*close(fd);exit(0);*/
} // main