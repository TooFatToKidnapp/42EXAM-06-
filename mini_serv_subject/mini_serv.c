
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>


typedef struct s_client {
	int fd;
	int id;
	struct s_client *next;
} t_client;

int sockfd, g_id;

t_client *clients = NULL; 

fd_set current , readfds, writefds;

char msg[200000] = {0};
char buff[2000080] = {0};


void fatal() {
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sockfd);
	exit(1);
}

int get_max_fd() {
int fd = sockfd;
t_client *tmp = clients;

while(tmp){
	if (tmp->fd > fd)
			fd = tmp->fd;

	tmp = tmp->next;


}
return fd;
}


void send_to_all(int fd) {
	t_client *tmp = clients;
	while (tmp) {
		if (fd != tmp->fd && FD_ISSET(tmp->fd, &writefds)) {
			if (0 > send(tmp->fd, buff, strlen(buff), 0))
				fatal();
		}
		tmp = tmp->next;
	}
	bzero(&buff , sizeof(buff));
}

void add_client() {
	struct sockaddr_in c;
	socklen_t len = sizeof(c);
	int cfd = accept(sockfd, (struct sockaddr*)&c, &len);
	if (0 > cfd)
		fatal();
	t_client *new = malloc(sizeof(t_client));
	if (!new)
		fatal();

	new->fd = cfd;
	new->id = g_id;
	new->next = NULL;
	if (!clients)
		clients = new;
	else {
		t_client *tmp = clients;
		while(tmp->next)
			tmp = tmp->next;
		tmp->next = new;
	}
	FD_SET(cfd, &current);
	sprintf(buff, "server: client %d just arrived\n", g_id);
	send_to_all(cfd);
	g_id++;

}

int get_id(int fd) {
	t_client *tmp = clients;

	while(tmp) {
		if (tmp->fd == fd) 
			return tmp->id;
		 tmp = tmp->next;
	}
	return -1;
}

void rm_client(int fd) {
	t_client *tmp = clients;
	t_client *save = NULL;
	
	sprintf(buff, "server: client %d just left\n", get_id(fd));
	send_to_all(fd);

	if (clients && clients->fd == fd ) {
		save = clients;
		clients = clients->next;
		FD_CLR(save->fd, &current);
		close(save->fd);
		free(save);
	}
	else {
		while(tmp) {
			if ( tmp->next && tmp->next->fd == fd) {
				save = tmp->next->next;
				FD_CLR(tmp->next->fd, &current);
        		close(tmp->next->fd);
       			free(tmp->next);
				tmp->next = save;
			}
			tmp = tmp->next;
		}
	}

}


void send_msg(int fd) {
	char tmp[200000] = {0};
	int i = -1;
	int j = -1;
	
	while(msg[++i] != '\0') {
		tmp[++j] = msg[i];
		if (msg[i] == '\n') {
			sprintf(buff, "client %d: %s", get_id(fd), msg);
			send_to_all(fd);
			bzero(&tmp, sizeof(tmp));
			j = -1;
		}
	}
	bzero(&msg, sizeof(msg));	
}

int main(int ac , char **av) {

	if (ac != 2) {
		write(2,"Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		return 1;
	}

	struct sockaddr_in server;
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	server.sin_port = htons(atoi(av[1]));

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (0 > sockfd)
		fatal();

	if (0 > bind(sockfd, (const struct sockaddr*)&server, sizeof(server)))
		fatal();
	if (0 > listen(sockfd, 100))
		fatal();

	FD_ZERO(&current);
	FD_SET(sockfd, &current);

	while (1) {
		readfds = writefds = current;
		if (0 > select(get_max_fd() +1 , &readfds, &writefds, NULL , NULL))
				continue;
		
		for(int fd = 0; fd <= get_max_fd(); ++fd) {
			if (FD_ISSET(fd, &readfds)) {
				if (fd == sockfd) {
					add_client();
					break;
				}
				int red = 1;
					while(red == 1 && msg[strlen(msg) == 0 ? 0 : strlen(msg) - 1 ] != '\n') {
						red = recv(fd, msg + strlen(msg), 1, 0);
						if (red <= 0)
							break;
					}
				if (red <= 0)
					rm_client(fd);
				send_msg(fd);
			}
				
		}
			
			
	}

	return 0;
	
}



