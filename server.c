#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <netinet/in.h>

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int         sockfd, max_fd;
int         count = 0;
struct      sockaddr_in servaddr, cli;
int         ids[700000];
char        *msgs[700000];
char        buf_read[7001], buf_write[42];
fd_set      curr, cpy_read, cpy_write;

void fatal()
{
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    exit(1);
}

void notify(int au, char *req) 
{
    for (int fd = 0; fd <= max_fd; fd++) {
        if (FD_ISSET(fd, &cpy_write) && fd != au)
            send(fd, req, strlen(req), 0);
    }
}

void register_client(int fd) 
{
    max_fd = fd > max_fd ? fd : max_fd;
    ids[fd] = count++;
    msgs[fd] = NULL;
    sprintf(buf_write, "server: client %d just arrived\n", ids[fd]);
    notify(fd, buf_write);
    FD_SET(fd, &curr);
}

void remove_client(int fd) 
{
    sprintf(buf_write, "server: client %d just left\n", ids[fd]);
    notify(fd, buf_write);
    if (msgs[fd])
        free(msgs[fd]);
    FD_CLR(fd, &curr);
    close(fd);
}

void send_msg(int fd) 
{
    char *msg;
    while (extract_message(&(msgs[fd]), &msg)) {
        sprintf(buf_write, "client %d: ", ids[fd]);
        notify(fd, buf_write);
        notify(fd, msg);
        free(msg);
    }
}

int main(int argc, char **argv) 
{
    if (argc != 2) {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        fatal();
    max_fd = sockfd;
    bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 
    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        fatal();
    if (listen(sockfd, SOMAXCONN) != 0)
        fatal();
    FD_ZERO(&curr);
    FD_SET(sockfd, &curr);
    while (1)
    {
        cpy_read = cpy_write = curr;
        if (select(max_fd + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
            fatal();
        for (int fd = 0; fd <= max_fd; fd++) {
            if (!FD_ISSET(fd, &cpy_read))
                continue;
            if (fd == sockfd) 
            {
                socklen_t len = sizeof(cli);
                int clientfd = accept(sockfd, (struct sockaddr *)&cli, &len);
                if (clientfd >= 0) {
                    register_client(clientfd);
                    break;
                }
            }
            else 
            {
                int ret = recv(fd, buf_read, 1000, 0);
                if (ret <= 0) {
                    remove_client(fd);
                    break ;
                }
                buf_read[ret] = '\0';
                msgs[fd] = str_join(msgs[fd], buf_read);
                send_msg(fd);
            }
        }
    }
}