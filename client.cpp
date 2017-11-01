#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <set>

using namespace std;

#include "duckchat.h"
#include "raw.h"

int cont;
char buffer[SAY_MAX];
char current[CHANNEL_MAX + 1];

text *message;
set <char>subscribed;

void login_request(int sock, struct addrinfo *p, const char *user);
void logout_request(int sock, struct addrinfo *p);
void join_request(int sock, struct addrinfo *p, const char *chann);
void leave_request(int sock, struct addrinfo *p, const char *chann);
void say_request(int sock, struct addrinfo* p, const char *text, const char *chann);
void list_request(int sock, struct addrinfo *p);
void who_request(int sock, struct addrinfo *p, const char *chann);
bool parse_input(int sock, struct addrinfo *p);
bool handle_server(int sock, struct addrinfo *p);

int main(int argc, char *argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Usage: ./client server_socket server_port username\n");
		return 1;
	}
	
	int check, sockfd;
	char *hostname, *port, *user;
	struct addrinfo server, *info, *rp;

	memset(&server, 0, sizeof server);
	
	server.ai_family   = AF_INET;
	server.ai_socktype = SOCK_DGRAM;
	
	hostname = argv[1];
	port     = argv[2];
	user     = argv[3]; 
	
	//check port number
	if (atoi(port) < 0 || atoi(port) > 65535) {
		printf("Port number must be greater than 0 and less than 65535.");
		exit(EXIT_FAILURE);
	}
	
	//check username
	if (strlen(user) > USERNAME_MAX) {
		printf("username is too long.");
		exit(EXIT_FAILURE);
	}
	
	check = getaddrinfo(hostname, port, &server, &info);
	if (check != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(check));
		exit(EXIT_FAILURE);
	}
	
	for (rp = info; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1) {
			perror("Client: socket() not successful \n");
			continue;
		}
		break;
	}
	
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	
	login_request(sockfd, rp, user);
	join_request(sockfd, rp, "Common");
		
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	message = (text *) malloc(322);
	memset(&buffer, 0, SAY_MAX);
	
	//printf(">");
	raw_mode();
	
	while(true) {
		parse_input(sockfd, rp);
		handle_server(sockfd, rp);
	}
	return 0;

}

void login_request(int sock, struct addrinfo *p, const char *user) {
	struct request_login login;
	memset(&login, '\0', sizeof(login));
	login.req_type = REQ_LOGIN;
	strncpy(login.req_username, user, USERNAME_MAX);
	
	if (sendto(sock, &login, sizeof(login), 0, p->ai_addr, p->ai_addrlen) < 0) {
		fprintf(stderr, "Unable to send login request.\n");
	}
}

void logout_request(int sock, struct addrinfo *p) {
	struct request_logout logout;
	
	memset(&logout, '\0', sizeof(logout));
	logout.req_type = REQ_LOGOUT;
	free(message);

	if (sendto(sock, &logout, sizeof(struct request_logout), 0, p->ai_addr, p->ai_addrlen) < 0) {
		fprintf(stderr, "Unable to send logout request.\n");
	}
}

void join_request(int sock, struct addrinfo *p, const char *chann) {
	struct request_join join;
	
	memset(&join, '\0', sizeof(join));
	join.req_type = REQ_JOIN;
	
	strncpy(join.req_channel, chann, CHANNEL_MAX);
	subscribed.insert(*chann);
	strncpy(current, chann, CHANNEL_MAX);
	
	if (sendto(sock, &join, sizeof(struct request_join), 0, p->ai_addr, p->ai_addrlen) < 0) {
		fprintf(stderr, "Unable to send join request.\n");
	}
}

void leave_request(int sock, struct addrinfo *p, const char *chann) {
	struct request_leave leave;
	
	memset(&leave, '\0', sizeof(leave));
	leave.req_type = REQ_LEAVE;
	
	strncpy(leave.req_channel, chann, CHANNEL_MAX);
	subscribed.erase(*chann);
	if (strcmp(current, chann) == 0) {
		memset(&current, '\0', sizeof current);
	}
	
	if (sendto(sock, &leave, sizeof(struct request_leave), 0, p->ai_addr, p->ai_addrlen) < 0) {
		fprintf(stderr, "Unable to send leave request.\n");
	}
}

void say_request(int sock, struct addrinfo *p, const char * text, const char * chann) {
	struct request_say say;

	memset(&say, '\0', sizeof(say));
	say.req_type = REQ_SAY;
	
	strncpy(say.req_text, text, SAY_MAX);
	strncpy(say.req_channel, chann, CHANNEL_MAX);
		
	if (sendto(sock, &say, sizeof(struct request_say), 0, p->ai_addr, p->ai_addrlen) < 0) {
		fprintf(stderr, "Unable to send say request.\n");
	}

}

void list_request(int sock, struct addrinfo *p) {
	struct request_list list;
	
	memset(&list, '\0', sizeof(list));
	list.req_type = REQ_LIST;
	
	if (sendto(sock, &list, sizeof(struct request_list), 0, p->ai_addr, p->ai_addrlen) < 0) {
		fprintf(stderr, "Unable to set request for list.\n");
	}
}

void who_request(int sock, struct addrinfo *p, const char *chann) {
	struct request_who who;
	
	memset(&who, '\0', sizeof(who));
	who.req_type = REQ_WHO;
	strncpy(who.req_channel, chann, CHANNEL_MAX);
	//char channel[CHANNEL_MAX];
    //strncpy(channel, text+5, strlen(text)-1);
	//strcpy(who.req_channel, channel); 

	if (sendto(sock, &who, sizeof(struct request_who), 0, p->ai_addr, p->ai_addrlen) < 0) {
		fprintf(stderr, "Unable to send who request.\n");
	}
}

bool parse_input(int sock, struct addrinfo *p) {	
	char c;
	char input[SAY_MAX];
	memset(input, '\0', SAY_MAX);
	input[strlen(input) - 1] = '\0';
	
	c = getchar();
	if ((int)c != -1) {
		if (c != '\n' && strlen(buffer) < SAY_MAX) {
			buffer[strlen(buffer)] = c;
			printf("%c", c);
		} else if (c == '\n') {
			char *param;
			printf("%c", c);
			param  = &buffer[1];
			if (buffer[0] == '/') {
				if ((strncmp(param, "join ", 5) == 0) && (buffer[6] != '\0')) {
					join_request(sock, p, &buffer[6]);
				} else if ((strncmp(param, "leave ", 6) == 0) && (buffer[7] != '\0')) {
					leave_request(sock, p, &buffer[7]);
				} else if (strncmp(param, "list", 4) == 0) {
					list_request(sock, p);
				} else if ((strncmp(param, "who ", 4) == 0) && (buffer[5] != '\0')) {
					who_request(sock, p, &buffer[5]);
				} else if ((strncmp(param, "switch ", 7) == 0) && (buffer[8] != '\0')) {
					if (subscribed.count(buffer[8]) == 1) {
						strncpy(current, &buffer[8], CHANNEL_MAX);
					} else {
						printf("Not currently subscribed to this channel.\n");
					}
				} else if (strncmp(param, "exit", 4) == 0) {
					logout_request(sock, p);
					close(sock);
					cooked_mode();
					exit(0);
				} else {
					printf("Unknown command. \n");
				}
			} else {
				say_request(sock, p, buffer, current);
			} 
			memset(&buffer, 0, SAY_MAX);
		}
	} 
	return true;
}

bool handle_server(int sock, struct addrinfo *p) {
	int dgram;
	text_say *serverSay;
	text_who *serverWho;
	text_list *serverList;
	text_error *serverError;	
	
	dgram = recvfrom(sock, message, 322, 0, p->ai_addr, &(p->ai_addrlen));
	
	if (dgram > 0) {
		message->txt_type = ntohl(message->txt_type);
		for (unsigned int i = 0; i < strlen(buffer); i++) {
			printf("\b \b");
		}	 
		if (message->txt_type == TXT_SAY) {
			serverSay = (text_say *) message;
			printf("[%s][%s]:%s\n> ", serverSay->txt_channel, serverSay->txt_username, serverSay->txt_text);	

		} else if (message->txt_type == TXT_LIST) {
			struct channel_info* channel;
			serverList = (text_list *) message;
			int no_channels = serverList->txt_nchannels;
			channel = serverList->txt_channels;

			printf("Existing channels: \n");
			for (int i = 0; i < no_channels; i++) {
				printf(" %s\n> ", (channel+i)->ch_channel);
			} 

		} else if (message->txt_type == TXT_WHO) {
			serverWho = (text_who *) message;
			
			int no_users = serverWho->txt_nusernames;

			printf("Users are on channel %s\n", serverWho->txt_channel);
			for (int i = 0; i < no_users; i++) {
				printf(" %s\n", (char*)serverWho->txt_users+(32*i));
			}
			
			printf("> ");
		} else if (message->txt_type == TXT_ERROR) {
			serverError = (text_error *) message;
			printf("Error: %s.\n", serverError->txt_error);

			printf("> ");
		}
		printf("%s", buffer); 
	}
	return false;
}	
