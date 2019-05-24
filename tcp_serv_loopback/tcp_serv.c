#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <strings.h>

#define SUCCESS 0
#define FAIL    -1
#define SESSION_NUMS 48
#define BACKLOG SESSION_NUMS
#define MAX_EVENTS SESSION_NUMS
#define MAX_CONNECTTION SESSION_NUMS

char buff[1024];

struct epoll_event ev, events[MAX_EVENTS];

int serv_start(int listen_port) {
	int addrlen, enable, epollfd, n, nfds;
	int accepted_sock, connection = 0, listen_sock;
	struct sockaddr_in addr;

	listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(listen_sock <= 0) {
		perror("open socket fail.");
		return FAIL;
	}

	enable = 1;
	if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(listen_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(listen_sock, (struct sockaddr *) &addr, sizeof(addr)) == -1)
	{
		perror("bind");
		return FAIL;
	}

	if( listen(listen_sock, BACKLOG) < 0 )
	{
		perror("listen");
		return FAIL;
	}
	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}

	for(;;) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}
		printf("epoll success! Detect %d events.\n", nfds);
		for (n = 0; n < nfds; ++n) {
			if (events[n].data.fd == listen_sock) {
				accepted_sock = accept(listen_sock,
					(struct sockaddr *) &addr, &addrlen);
				if (accepted_sock == -1) {
					perror("accept");
					exit(EXIT_FAILURE);
				}
				if(connection){
					close(accepted_sock);
					continue;
				}
				else
					connection = accepted_sock;
				ev.events = EPOLLIN;
				ev.data.fd = connection;
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connection,
					&ev) == -1) {
					perror("epoll_ctl: connection");
					exit(EXIT_FAILURE);
				}
			} else {
				int cnt = recv(events[n].data.fd, buff, sizeof(buff), 0);
				if(cnt > 0) {
					cnt = send(events[n].data.fd, buff, cnt, 0);
					printf("loopback %d bytes\n", cnt);
				}
				else {
					close(events[n].data.fd);
					connection = 0;
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {
	if(argc > 1)
		serv_start(atoi(argv[1]));
	else
		printf("usage: program_name listen_port \n");
}
