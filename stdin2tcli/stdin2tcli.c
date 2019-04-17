#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/time.h>

#define LOCA_PORT 5010
#define PEER_PORT 4001
#define PEER_IP "127.0.0.1"

#define SUCCESS 0
#define FAIL 1

#define DBG(...) printf("==DBG== %-20s:%d ",__func__,__LINE__); printf(__VA_ARGS__);
#define BUFFER_SIZE (16*1024)
char buf[BUFFER_SIZE];

int is_connected(int sock_fd);
int wait_connected(int sock_fd, time_t sec_timeout, suseconds_t usec_timeout);
int try_nblking_connect(int sock_fd, struct sockaddr_in *addr);
int connected_process(int sock_fd, time_t sec_timeout, suseconds_t usec_timeout);
int set_no_linger(int sock_fd);

int is_connected(int sock_fd)
{
	struct sockaddr addr;
	socklen_t addrlen;

	if(getpeername(sock_fd, &addr, &addrlen) < 0)
	{
		perror("connection");
		return 0;
	}
	return 1;
}

int wait_connected(int sock_fd, time_t sec_timeout, suseconds_t usec_timeout)
{
	struct timeval tout;
	int i, max_fd, ret;
	fd_set w_fds;


	for(i = 0; i < 3; i++) {
		tout.tv_sec = sec_timeout;
		tout.tv_usec = usec_timeout;

		FD_ZERO(&w_fds);
		FD_SET(sock_fd, &w_fds);
		max_fd = sock_fd > STDIN_FILENO ? sock_fd : STDIN_FILENO;

		if((ret = select(max_fd+1, NULL, &w_fds, NULL, &tout)) <= 0)
		{
			printf("socket connecting try %d.\n",i+1);
		}
		else
		{
			if(FD_ISSET(sock_fd, &w_fds))
			{
				if(is_connected(sock_fd))
				{
					printf("socket connect succeed.\n");
					return SUCCESS;
				}
			}
		}
	}
	return FAIL;
}

int try_nblking_connect(int sock_fd, struct sockaddr_in *addr)
{
	int ret = connect(sock_fd, (struct sockaddr *) addr, sizeof(struct sockaddr));
	if (0 == ret)
	{
		printf("socket connect succeed immediately.\n");
	}
	else
	{
		printf("get the connect result by select().\n");
		if (errno == EINPROGRESS)
		{
			ret = wait_connected(sock_fd, 2, 0);
		}
	}
	return ret;
}

int connected_process(int sock_fd, time_t sec_timeout, suseconds_t usec_timeout)
{
	struct timeval tout;
	int max_fd;
	fd_set r_fds, w_fds, e_fds;

	tout.tv_sec = sec_timeout;
	tout.tv_usec = usec_timeout;

	FD_ZERO(&r_fds);
	FD_ZERO(&w_fds);
	FD_ZERO(&e_fds);

	FD_SET(STDIN_FILENO, &r_fds);
	FD_SET(STDIN_FILENO, &e_fds);
	FD_SET(sock_fd, &r_fds);
	FD_SET(sock_fd, &w_fds);

	max_fd = sock_fd > STDIN_FILENO ? sock_fd : STDIN_FILENO;

	if(select(max_fd+1, &r_fds, &w_fds, &e_fds, &tout) > 0)
	{
		if(FD_ISSET(sock_fd, &r_fds))
		{
			if(recv(sock_fd, buf, sizeof(buf), 0) <= 0)
			{
				return FAIL;
			}
		}
		if(FD_ISSET(STDIN_FILENO, &r_fds))
		{
			if(FD_ISSET(sock_fd, &w_fds))
			{
				int ret;

				ret = fread(buf, sizeof(buf), sizeof(char), stdin);
				if (ret < 0)
				{
					perror("fread error\n");
					return FAIL;
				}
				ret = send(sock_fd, buf, strlen(buf), 0);
				if (ret < 0)
				{
					perror("send error\n");
					return FAIL;
				}
			}
		}
	}
	return SUCCESS;
}

int set_no_linger(int sock_fd)
{
	struct linger linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;

	setsockopt(sock_fd, SOL_SOCKET, SO_LINGER,
		(char *) &linger, sizeof(linger));
	return SUCCESS;
}


int main(int argc, char **argv)
{
	int sock_fd;
	int flags, retry_cnt, ret, enable;
	struct sockaddr_in addr;

	sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sock_fd <= 0){
		perror("open socket fail.");
		return FAIL;
	}

	enable = 1;
	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	    perror("setsockopt(SO_REUSEADDR) failed");

	flags = fcntl(sock_fd, F_GETFL, 0);
	fcntl(sock_fd, F_SETFL, flags|O_NONBLOCK);
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(LOCA_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
		perror("bind");

	addr.sin_port   = htons(PEER_PORT);
	addr.sin_addr.s_addr = inet_addr(PEER_IP);

	for(retry_cnt = 0; retry_cnt < 3; retry_cnt++) {
		ret = try_nblking_connect(sock_fd, &addr);
		if(ret == 0){
			break;
		}
	}
	if(ret == 0)
		connected_process(sock_fd, 0, 10000);

	set_no_linger(sock_fd);

	usleep(1500000);
	close(sock_fd);

	return ret;
}