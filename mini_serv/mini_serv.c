#include <stdio.h>     
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>

typedef struct s_client {
		int fd,id;
		struct s_client *next;
}	t_client;

t_client *clients = NULL;

int sockfd,g_id;

int max_fd;

char msg[200000] = {0};
char buff[200090] = {0};

fd_set current, readfds, writefds;

#define RUNNING 1

void fatal() {
	write(2,"Fatal error\n", strlen("Fatal error\n"));
	close(sockfd);
	exit(1);
}

int get_max_fd() {
	int fd = sockfd;
	t_client *tmp = clients;
	while (tmp) {
		if (tmp->fd > fd)
				fd = tmp->fd;
		tmp = tmp->next;
	}
	return fd;
}

void share_to_all(int fd) {
	t_client *tmp = clients;
	while (tmp) {
		if (tmp->fd != fd && FD_ISSET(tmp->fd, &writefds)) {
			 send(tmp->fd, buff, strlen(buff), 0);
		}
		tmp = tmp->next;
	}
	bzero(&buff, sizeof(buff));
}

void add_client() {
	struct sockaddr_in c;
	socklen_t l = sizeof(c);
	int cfd = accept(sockfd, (struct sockaddr*)&c, &l);
	if (0 > cfd)
		 return;
	sprintf(buff, "server: client %d just arrived\n", g_id);
    share_to_all(cfd);
	if (cfd > max_fd)
			max_fd = cfd;
	t_client *new;
	new = malloc(sizeof(t_client));
	t_client *tmp = clients;
	if (!new)
		fatal();
	new->fd = cfd;
	new->id = g_id;
	new->next = NULL;
	FD_SET(cfd, &current);
	if (clients == NULL) 
		clients = new;
	else {
		while (tmp->next) { 
			tmp = tmp->next;
		}	
		tmp->next = new;
	}
	g_id++;
}

int get_id(int fd) {
	t_client *tmp = clients;
	while (tmp) {
		if (tmp->fd == fd)
			return tmp->id;
		tmp = tmp->next;
	}
	return -11;
}

void rm_client(int fd) {
	sprintf(buff, "server: client %d just left\n", get_id(fd));
    share_to_all(fd);
	t_client *tmp = clients;
	t_client *save = NULL;
	if (clients && clients->fd == fd) {
		save = clients->next;
		close(clients->fd);
		FD_CLR(clients->fd, &current);
		free(clients);
		clients = save;
		if (max_fd == fd)
				max_fd = get_max_fd();
		return;
	}
	else {
		while(tmp) {
			if (tmp->next && tmp->next->fd == fd) {
				save = tmp->next->next;
				close(tmp->next->fd);
				FD_CLR(tmp->next->fd, &current);
				free(tmp->next);
				tmp->next = save;
				if (max_fd == fd)
                	max_fd = get_max_fd();
				return;
			}
			tmp = tmp->next;
		}
	}
}

void send_client_msg(int fd) {
//	puts("here");
	char  tmp[200000] = {0};
	int i = -1;
	int j = -1;
	while(msg[++i] != '\0') {
		tmp[++j] = msg[i];
		if(msg[i] == '\n') {
			sprintf(buff, "client %d: %s", get_id(fd), tmp);
			share_to_all(fd);
			bzero(&tmp, sizeof(tmp));
			j = -1;
		}
	}
	bzero(&msg, sizeof(msg));
}


int main (int ac , char **av) {
	
	if (ac != 2) {
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		return 1;
	}

	sockfd = socket(AF_INET ,SOCK_STREAM, 0);
	if (0 > sockfd)
			fatal();

	struct sockaddr_in s;
	bzero(&s, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	s.sin_port = htons(atoi(av[1]));
	
	if (0 > bind(sockfd, (const struct sockaddr*)&s, sizeof(s))) 
		fatal();
	if (0 > listen(sockfd, 100))
		fatal();

	FD_ZERO(&current);
	FD_SET(sockfd, &current);
	
	max_fd = sockfd;
	while(RUNNING)
	{
		readfds = writefds = current;
		if (0 > select(max_fd + 1, &readfds, &writefds, NULL , NULL))
				continue;
		for(int fd = 0; fd <= max_fd; fd++) {
			if (FD_ISSET(fd, &readfds)) {
				if (fd == sockfd) {
					add_client();
					break;
				}
				int red = 1;
				size_t l = strlen(msg);
				while (red == 1 && msg[l == 0 ? 0 : l - 1] != '\n') {
					red = recv(fd, msg + l, 1, 0);
					l = strlen(msg);
					if (red <= 0)
						break;
				}
				if (red <= 0) {
					rm_client(fd);
					break;
				}
				send_client_msg(fd);
			}
		}

	}


	return 0;
}
