
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <iostream>
#include <string>
#include <map>
#include <utility>
#include <vector>
#include "duckchat.h"

using namespace std;

int socket_connect(char *ip, char *port);
int login_request(struct request_login *l);
int logout_request(struct request_logout *l);
int join_request(struct request_join *j);
int leave_request(struct request_leave *l);
int list_request(struct request_list *l);
int who_request(struct request_who *w);
int say_request(struct request_say *s);
int parse_input(struct request *r, int port);
int check_addr(struct sockaddr_in, struct sockaddr_in);
int is_valid();
string get_user();
string string_addr(struct sockaddr_in);
void error(struct sockaddr_in addr, string message);
struct sockaddr_in get_addr_struct();

int sockfd;
struct sockaddr server; 
struct addrinfo *rp;
socklen_t fromLen = sizeof(server);
map<string, vector<string> > userTalk;
map<string, vector<pair<string, struct sockaddr_in> > > channelTalk;
multimap<string, struct sockaddr_in> user2addr;
vector<string> channels;

int main (int argc, char **argv) {	
	char *ip, *port;
	rp = NULL;
	sockfd = 0;
	
	if (argc != 3) {
		printf("Usage: ./server server_socket server_port.\n");
		return 0;
	}
	
	ip = argv[1];
	port = argv[2];
	
	memset((char *) &server, 0, sizeof(server));
	socket_connect(ip, port);
	
	while (1) {
		int num = 0;
		char *buffer;
		struct request *requesting; 
		
		buffer = new char[1024];
		requesting = (struct request *)malloc(sizeof(struct request *) + 1024);
		num = recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr *)&server, &fromLen);
		if (num > 0) {
			printf("%d bytes\n", num);		
			requesting = (request *)buffer;
			parse_input(requesting, num);
		}
		
		delete[] buffer;
	} 
	
	return 0;
}

int socket_connect(char *ip, char *port) {
	int result;
	struct addrinfo address;
	memset(&address, 0, sizeof address);
	address.ai_family   = AF_INET;
	address.ai_socktype = SOCK_DGRAM;
	address.ai_flags    = AI_PASSIVE;
	
	result = 0;
	if ((result = getaddrinfo(ip, port, &address, &rp)) != 0) {
		return false;
	} 
	
	if ((sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
		return false;
	}
	
	if ((bind(sockfd, rp->ai_addr, rp->ai_addrlen)) == -1) {
		return false;
	}
	
	return true;
}

int login_request(struct request_login *l) {
	string username;
	struct sockaddr_in address;
	
	username = l->req_username;
	user2addr.insert(user2addr.end(), pair<string, struct sockaddr_in> (username, address));
	return 0;
}

int logout_request(struct request_logout *l) {
	string username;
	map<string, vector<string> >::iterator iter;
	multimap<string, struct sockaddr_in>::iterator sockIter;
	
	username = get_user();
	sockIter = user2addr.find(username);
	user2addr.erase(sockIter);
	
	iter = userTalk.find(username);
	if (iter != userTalk.end()) {
		userTalk.erase(username);
	}
	
	for (int i = 0; i < channels.size(); i++) {
		map<string, vector<pair<string, struct sockaddr_in> > >::iterator it;
		vector<pair<string, struct sockaddr_in> > users;
		it = channelTalk.find(channels[i]);
		users = it->second;
		
		for (int j = 0; j < users.size(); j++) {
			if (users[j].first == username) {
				users.erase(users.begin() + j);
			}
		}
		
		channelTalk.erase(it);
		channelTalk.insert(pair<string, vector<pair<string, struct sockaddr_in> > >(channels[i], users));
	}
	return 0;
}

int join_request(struct request_join *j) {
	string channel, username;
	struct sockaddr_in address;
	map<string, vector<pair<string, struct sockaddr_in> > >::iterator iter;
	vector <pair<string, struct sockaddr_in> > users;
	
	channel  = (string)j->req_channel;
	username = get_user();
	address  = get_addr_struct();
	iter = channelTalk.find(channel);
	
	if (iter == channelTalk.end()) {
		users.insert(users.begin(), pair<string, struct sockaddr_in>(username, address));
		channelTalk.insert(pair<string, vector<pair<string, struct sockaddr_in> > >(channel, users));
		channels.push_back(channel);
	} else {
		iter = channelTalk.find(channel);
		users = iter->second;
		for (int i = 0; i < users.size(); i++) {
			if (users[i].first == username) {
				return -1;
			}
		}
		users.insert(users.begin(), pair<string, struct sockaddr_in>(username, address));
		channelTalk[channel] = users;
	}
	vector<string> channelTalk = userTalk[username];
	channelTalk.insert(channelTalk.begin(), channel);
	userTalk[username] = channelTalk;
	
	return 0;
}

int leave_request(struct request_leave *l) {
	string username, channel;
	struct sockaddr_in addr, address;
	multimap<string, struct sockaddr_in>::iterator mIter;
	map <string, vector<pair<string, struct sockaddr_in> > >::iterator iter;
	vector<pair<string, struct sockaddr_in> > vect;
	
	username = get_user();
	channel  = (string)(l->req_channel);
	addr  = get_addr_struct();
	mIter = user2addr.find(username);
	address  = mIter->second;
	iter = channelTalk.find(channel);
	
	if ((iter = channelTalk.find(channel)) == channelTalk.end()) {
		return -1;
	}
	
	vect = iter->second;
	for (int i = 0; i < vect.size(); i++) {
		if (vect[i].first == username) {
			if (check_addr(vect[i].second, addr) == 0) {
				vect.erase(vect.begin() + i);
			}
		}
	}
	
	channelTalk.erase(iter);
	if (vect.size() != 0) {
		channelTalk.insert(pair<string, vector<pair<string, struct sockaddr_in> > > (channel, vect));
		return 0;
	} else {
		for (int i = 0; i < channels.size(); i++) {
			if (channels[i] == channel) {
				channels.erase(channels.begin()+i);
			}
		}
	}
	vector<string> channelTalkVect = userTalk[username];
	for (int i = 0; i < channelTalkVect.size(); i++) {
		if (channelTalkVect[i] == channel) {
		channelTalkVect.erase(channelTalkVect.begin() + i);
		}	
	}
	
	userTalk[username] = channelTalkVect;
	return 0;
}

int list_request(struct request_list *l) {
	int  numCh;
	string username;
	struct sockaddr_in address;
	struct text_list *message;
	multimap<string, struct sockaddr_in>::iterator iter;
	
	username = get_user();
	iter  = user2addr.find(username);
	numCh = channels.size();
	address = iter->second;
	message  = (struct text_list *)malloc((sizeof(struct text_list) + (numCh * sizeof(struct channel_info))));
	message->txt_type = TXT_LIST;
	message->txt_nchannels = numCh;
	
	for (int i = 0; i < channels.size(); i++) {
		strcpy(((message->txt_channels) + i)->ch_channel, channels[i].c_str());
	}
	
	if (sendto(sockfd, message, (sizeof(struct text_list) + (numCh * sizeof(struct channel_info))), 0, (struct sockaddr *)&address, sizeof(struct sockaddr)) < 0) {
		return -1;
	}
	
	free(message);
	return 0;
}

int who_request(struct request_who *w) {
	int numCh;
	const char *str;
	string username, channel;
	struct sockaddr_in address;
	multimap<string, struct sockaddr_in>::iterator mulIter;
	map<string, vector<pair<string, struct sockaddr_in> > >::iterator mapIter;
	vector<pair<string, struct sockaddr_in> > vect;
	struct text_who *message;
	
	username = get_user();
	channel  = (string)(w->req_channel);
	mulIter  = user2addr.find(username);
	mapIter  = channelTalk.find(channel);
	numCh = (mapIter->second).size();
	vect  = mapIter->second;
	address = mulIter->second;
	message = (struct text_who *)malloc(sizeof(struct text_who) + (numCh* sizeof(struct user_info)));
	
	message->txt_type = TXT_WHO;
	message->txt_nusernames = numCh;
	str = channel.c_str();
	strcpy(message->txt_channel, str);
	for (int i = 0; i < vect.size(); i++) {
		strcpy(((message->txt_users) + i)->us_username, vect[i].first.c_str());
	}
	
	if (sendto(sockfd, message, (sizeof(struct text_who) + (numCh * sizeof(struct user_info))), 0, (struct sockaddr *)&address, sizeof(struct sockaddr)) < 0) {
		return -1;
	}
	free(message);
	return 0;
}

int parse_input(struct request *r, int port) {
	int host, result;
	
	host = ntohl(r->req_type);
	/*if (host != 0) {
		if (is_valid() == -1) {
			printf("Invalid address.\n");
			return -1;
		}
	}*/
	if (host == REQ_LOGIN) {
		if (sizeof(struct request_login) == port) {
			printf("Login request.\n");
			result = login_request((struct request_login *) r);
		} else {
			printf("Login request failed.\n");
		}
	} else if (host == REQ_LOGOUT) {
		if (sizeof(struct request_logout) == port) {
			printf("Logout request.\n");
			result = logout_request((struct request_logout *) r);
		} else {
			printf("Logout request failed.\n");
		}
	} else if (host == REQ_JOIN) {
		if (sizeof(struct request_join) == port) {
			printf("Join request.\n");
			result = join_request((struct request_join *) r);
		} else {
			printf("Join request failed.\n");
		}
	} else if (host == REQ_LEAVE) {
		if (sizeof(struct request_leave) == port) {
			printf("Leave request.\n");
			result = leave_request((struct request_leave *) r);
		} else {
			printf("Leave request failed.\n");
		}
	} else if (host == REQ_SAY) {
		if (sizeof(struct request_say) == port) {
			printf("Say request.\n");
			result = say_request((struct request_say *) r);
		} else {
			printf("Say request failed.\n");
		}
	} else if (host == REQ_LIST) {
		if (sizeof(struct request_list) == port) {
			printf("List request.\n");
			result = list_request((struct request_list *) r);
		} else {
			printf("List request failed.\n");
		}
	} else if (host == REQ_WHO) {
		if (sizeof(struct request_who) == port) {
			printf("Who request.\n");
			result =  who_request((struct request_who *) r);
		} else {
			printf("Who request failed.\n");
		}
	}
	return result;	
}


int say_request(struct request_say *s) {
	string channel, message, username;
	struct sockaddr_in from;
	map<string, vector<pair<string, struct sockaddr_in> > >::iterator iter;
	vector<pair<string, struct sockaddr_in> > tmp;
	
	channel  = s->req_channel;
	message  = s->req_text;
	username = get_user();
	from = get_addr_struct();
	iter = channelTalk.find(channel);
	
	if (iter == channelTalk.end()) {
		return -1;
	} 
	
	tmp = iter->second;
	
	for (int i = 0; i < tmp.size(); i++) {
		struct sockaddr_in address;
		struct text_say data;
		
		address = tmp[i].second;
		//data    = (struct text_say *)malloc(sizeof(struct text_say) + sizeof(message));
		data.txt_type = TXT_SAY;
		
		strncpy(data.txt_username, username.c_str(), sizeof(username));
		strncpy(data.txt_text, message.c_str(), sizeof(message));
		strncpy(data.txt_channel, channel.c_str(), sizeof(channel));
		
		if ((sendto(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&address, sizeof(struct sockaddr *))) < 0)	{
			printf("Say sendto error.\n");
			return -1;
		}
		//free(data);	
	}
	tmp.clear();
	return 0;
}

int is_valid() {
	struct sockaddr_in *address = (struct sockaddr_in *)&server;
	multimap<string, struct sockaddr_in>::iterator iter;
	for (iter = user2addr.begin(); iter != user2addr.end(); iter++) {
		if (check_addr(iter->second, *address) == 0) {
			return 0;
		}
	}
	return -1;
}

int check_addr(struct sockaddr_in addr1, struct sockaddr_in addr2) {
	char *address1, *address2;
	string string1, string2;
	
	address1 = (char *)malloc(sizeof(char) * 1024);
	address2 = (char *)malloc(sizeof(char) * 1024);
	
	inet_ntop(AF_INET, &(addr1.sin_addr), address1, 1024);
	inet_ntop(AF_INET, &(addr2.sin_addr), address2, 1024);
	
	string1 = address1;
	string2 = address2;
	
	if (string1 == string2) {
		int port1, port2;
		port1 = addr1.sin_port;
		port2 = addr2.sin_port;
		if (port1 == port2) {
			return 0;
		}
	}
	return -1;
}

string get_user() {
	string tmp = "";
	struct sockaddr_in *address;
	
	address = (struct sockaddr_in *)&server;
	multimap<string, struct sockaddr_in>::iterator iter;
	
	for (iter = user2addr.begin(); iter != user2addr.end(); iter++) {
		if (check_addr(iter->second, *address) == 0) {
			tmp = iter->first;
		}
	}
	return tmp;
}

string string_addr(struct sockaddr_in addr) {
	char *address;
	string ret;
	
	address = (char *)malloc(sizeof(char) *1024);
	inet_ntop(AF_INET, &(addr.sin_addr), address, 1024);
	ret = address;
	return ret; 
}

/*void error(struct sockaddr_in addr, string msg) {
	struct sockaddr_in send_socket;
	struct text_error *message;
	const char *err;
	int result;
	
	message.txt_type = TXT_ERROR;
	err = msg.c_str();
	strcpy(message.txt_error, err);
	send_socket = addr;
	
	strncpy(message->txt_error, err.c_str(), SAY_MAX);
	if (sendto(sockfd, &message, sizeof message, 0, (struct sockaddr *)&addr, sizeof send_socket) < 0) {
		return -1;
	}
	return 0;
}
*/
struct sockaddr_in get_addr_struct() {
	struct sockaddr_in *address = (struct sockaddr_in *)&server;
	return *address;
}
>>>>>>> b8a8405d56cd3e8b481adabadc14d13f9b96a7ec
