#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include "Api_mysql.h"

bool g_ctrl_c_pressed = false;
void sig_handler(int a_signal)
{
	write(0, "\nCtrl^C pressed in sig handler\n", 32);
	g_ctrl_c_pressed = true;
}

int SendFrameData(int a_fd_socket, unsigned char a_message_id, const char *ap_body_data, unsigned char a_body_size)
{
	char *p_send_data = (char *)malloc(3 + a_body_size);

	*p_send_data = 27;
	*(unsigned char *)(p_send_data + 1) = a_message_id;
	*(unsigned char *)(p_send_data + 2) = a_body_size;

	memcpy(p_send_data + 3, ap_body_data, a_body_size);

	int temp_size = write(a_fd_socket, p_send_data, 3 + a_body_size);
	if (temp_size < 0)
	{
		printf("Disconnected Client!!\n");
		close(a_fd_socket);
		return -1;
	}

	return 1;
}

int GetFrameData(int a_fd_socket, unsigned char *ap_message_id, char **ap_body_data, unsigned char *ap_body_size)
{
	int temp_size, state = -1;
	char key;
	temp_size = read(a_fd_socket, &key, 1);
	if (temp_size < 0)
	{
		printf("Disconnected Client!!\n");
	}
	else if (key == 27)
	{
		temp_size = read(a_fd_socket, ap_message_id, 1);
		if (temp_size < 0)
		{
			printf("Disconnected Client!!\n");
		}
		else
		{
			temp_size = read(a_fd_socket, ap_body_size, 1);
			if (temp_size < 0)
			{
				printf("Disconnected Client!!\n");
			}
			else
			{
				if (*ap_body_size > 0)
				{
					*ap_body_data = (char *)malloc(*ap_body_size);
					temp_size = read(a_fd_socket, *ap_body_data, *ap_body_size);
					if (temp_size < 0)
					{
						printf("Disconnected Client!!\n");
					}
					else
					{
						return 1;
					}
				}
				else
				{
					*ap_body_data = NULL;
					return 1;
				}
			}
		}
	}
	else
	{
		printf("Invalid Client!!\n");
		state = -2;
	}

	close(a_fd_socket);
	return state;
}

int main()
{
	char strQuery[128];
	fd_set rfds;
	struct timeval tv;
	unsigned char message_id, body_size;
	char *p_body_data;

	struct sockaddr_in serv_addr, cli_addr;
	struct sigaction sig_struct;
	sig_struct.sa_handler = sig_handler;
	sig_struct.sa_flags = 0;
	sigemptyset(&sig_struct.sa_mask);

	if (sigaction(SIGINT, &sig_struct, NULL) == -1)
	{
		printf("Problem with sigaction\n");
		exit(1); //프로세스 종료
	}

	int fd_listen_socket = socket(AF_INET, SOCK_STREAM, 0), temp_client_info_size, fd_client_socket, retval;
	if (fd_listen_socket < 0)
	{
		printf("Socket system error!!\n");
		return 0;
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));
	temp_client_info_size = sizeof(cli_addr);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_addr.sin_port = htons(29001);

	if (bind(fd_listen_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Bind error!!\n");
		close(fd_listen_socket);
	}

	listen(fd_listen_socket, 5);

	while (1)
	{
		printf("waiting for new client\n");

		fd_client_socket = accept(fd_listen_socket, (struct sockaddr *)&cli_addr, (socklen_t *)&temp_client_info_size);
		if (fd_client_socket < 0)
		{
			printf("Error on accept!!\n");
		}
		else
		{
			printf("Client connected!!\n");
			tv.tv_sec = 0;
			tv.tv_usec = 50000;

			while (1)
			{
				FD_ZERO(&rfds);
				FD_SET((long unsigned int)fd_client_socket, &rfds);

				retval = select(fd_client_socket + 1, &rfds, NULL, NULL, &tv);
				if (retval != 0 && retval != -1)
				{
					if (1 == GetFrameData(fd_client_socket, &message_id, &p_body_data, &body_size))
					{
						if (p_body_data != NULL)
						{
							memset(strQuery, 0, sizeof(strQuery));
							switch (message_id)
							{
								case 1:
									// INSERT INTO Temperature VALUES ('정보를 보낸 노드 번호', '시간', %s);
									sprintf(strQuery, "INSERT INTO Temperature VALUES (%s)", p_body_data);
									Single_Query(DATABASE, strQuery);
									//SendFrameData(fd_client_socket, 1, "STRING", sizeof("STRING"));
									break;
								case 2:
									sprintf(strQuery, "INSERT INTO Humidity VALUES (%s)", p_body_data);
									Single_Query(DATABASE, strQuery);
									break;
							}
							free(p_body_data);
						}
					}
					else
						break;
				}

				if (g_ctrl_c_pressed)
					break;
			}
		}

		if (g_ctrl_c_pressed)
		{
			printf("Ctrl^C Pressed\n");
			printf("Stop server sevice!!\n");
			break;
		}
	}

	close(fd_listen_socket);
	return 0;
}
