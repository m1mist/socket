#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int main()
{
	//1.通信套接字
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		return -1;
	}

	//2.连接服务器
	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(7890);
	inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr.s_addr);
	int ret = connect(fd,(struct sockaddr*)&saddr, sizeof(saddr));
	if (ret == -1) {
		perror("connect");
		return -1;
	}
	//3.通信
	int number = 0;
    while (1) 
	{
		char buff[1024];
		sprintf(buff, "Hello,%d...", number++);
		send(fd, buff, strlen(buff)+1, 0);

		//接收
		memset(buff, 0, sizeof(buff));
		int len = recv(fd, buff, sizeof(buff), 0);
		if(len > 0)
		{
			printf("server says:%s\n", buff);
		}
		else if(len == 0)
		{
			perror("server disconnected");
			break;
		}
		else
		{
			perror("recv");
			break;
		}
		sleep(1);
	}
	close(fd);
	return 0;
}
