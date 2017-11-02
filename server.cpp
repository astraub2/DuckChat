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
#include <set> 
#include <utility>
#include <vector>
#include "duckchat.h"
//defined
#define BUFFER 1024
using namespace std;

//globals
int 			sockfd;
struct sockaddr server;
struct addrinfo *rp;

map<string,vector<string> > 							userTalk;
multimap<string,struct sockaddr_in> 					user2addr;
map<string,vector<pair<string,struct sockaddr_in> > >   channelTalk;
vector<string> channels;


//methods
int socket_connect(char *ip, char *port);
int login_request(struct request_login *log);
int logout_request();
int join_request(struct request_join *join);
int leave_request(struct request_leave *leave);
int list_request();
int who_request(struct request_who *who);
int say_request(struct request_say *say);
int handle_input(struct request *req, int rec);
int is_valid();
int check_equal(struct sockaddr_in addr1, struct sockaddr_in addr2);
void error_request(struct sockaddr_in sock, string msg);
string get_current_user();
struct sockaddr_in address_struct();


//program
int main(int argc, char **argv) {
	char *ip, *port;
	socklen_t from;
	
	if (argc != 3) {
		fprintf(stderr, "Usage: ./server server_socket server_port\n");
	}
	
	sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("Server: socket() not successful.\n");
		exit(EXIT_FAILURE);
	}
	
	from = sizeof(server);   
    rp   = NULL;
    sockfd = 0;
    ip     = argv[1];
    port   = argv[2];
    
    socket_connect(ip, port);
    
    while (true) {
        char *buf = new char[BUFFER];
        struct request *requests = (struct request*)malloc(sizeof(struct request*) + BUFFER);  
        int bal = 0;
        bal = recvfrom(sockfd, buf, BUFFER, 0, (struct sockaddr*)&server, &from);
        if(bal > 0) {
            requests = (request*) buf;
            handle_input(requests, bal);
        } 
       delete[] buf;   
    }
    return 0;
}

int socket_connect(char* ip, char* port) {
    int result, listener, set;
    struct addrinfo address;
    
    set = 1;
    memset(&address, 0, sizeof address);
    address.ai_family   = AF_INET;
    address.ai_socktype = SOCK_DGRAM;
    address.ai_flags    = AI_PASSIVE;
    
    result = 0;
    if((result = getaddrinfo(ip, port, &address, &rp))!= 0) {
        printf("Server: getaddrinfo not successful.\n");
        return 0;
    }
    
    if((sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
        printf("Server: socket() not successful.\n");
        return 0;
    }
    
    if(bind(sockfd, rp->ai_addr, rp->ai_addrlen) == -1) {
        printf("Server: bind failre.\n");
        return 0;
    }
    
    return 1;
}

int login_request(struct request_login *log) {
    string username; 
    struct sockaddr_in address;
    map<string,vector<pair<string,struct sockaddr_in> > >::iterator iter;
    vector<pair<string,struct sockaddr_in> > users;
	vector<string> userTalkVect;;
    
    username = log->req_username;
    address  = address_struct();
    
    user2addr.insert(user2addr.end(), pair<string, struct sockaddr_in>(username, address));
	iter = channelTalk.find("Common");

    if(iter == channelTalk.end()) {
        channelTalk.insert(pair<string,vector<pair<string,struct sockaddr_in> > >("Common", users));
        channels.push_back("Common");
    }
    
    iter = channelTalk.find("Common");
    users = iter->second;
    users.insert(users.begin(), pair<string,struct sockaddr_in>(username, address));
    channelTalk["Common"] = users;

    userTalkVect.insert(userTalkVect.begin(),"Common");
    userTalk.insert(pair<string,vector<string> >(username, userTalkVect));    
    
    cout << username << ": recv Request Login" << endl;
    
    return 0;
}

int logout_request() {
    string username;
    multimap<string, struct sockaddr_in>::iterator sockIter;
    map<string,vector<string> >::iterator iter;
    
    username = get_current_user();
	sockIter = user2addr.find(username);
    user2addr.erase(sockIter);
    
    cout << username << ": recv Request Logout" << endl;
    
    iter = userTalk.find(username);
    if(iter != userTalk.end()) {
        userTalk.erase(username);
    }

    for(int i = 0; i < channels.size(); i++) {
        map<string,vector<pair<string,struct sockaddr_in> > >::iterator channelIter;
        vector<pair<string,struct sockaddr_in> > users;
        
        channelIter = channelTalk.find(channels[i]);
        users = channelIter->second;
        for(int j = 0; j < users.size(); j++) {
            if(users[j].first == username) {
                //cout << "deleting user: " << users[j] << " \n";
                users.erase(users.begin()+j);
            }
        }
        channelTalk.erase(channelIter);
        channelTalk.insert(pair<string,vector<pair<string,struct sockaddr_in> > >(channels[i],users));
    }
    return 0;
}

int join_request(struct request_join *join) {
    string channel, username; 
    struct sockaddr_in address; 
    map<string,vector<pair<string,struct sockaddr_in> > >::iterator iter;
    vector<pair<string,struct sockaddr_in> > users;
    
    channel  = (string)join->req_channel;
    username = get_current_user();
    address  = address_struct();
    iter = channelTalk.find(channel);

    cout << username << ": recv Request Join" << endl;
   
    if(iter == channelTalk.end()) {
        users.insert(users.begin(), pair<string,struct sockaddr_in>(username,address));
        channelTalk.insert(pair<string,vector<pair<string,struct sockaddr_in> > >(channel, users));
        channels.push_back(channel);
    } else {
        iter = channelTalk.find(channel);
        users = iter->second;
        for(int i = 0; i < users.size(); i++) {
        	if(users[i].first == username) {
                return -1;
            }
        }
        users.insert(users.begin(), pair<string,struct sockaddr_in>(username, address));
        channelTalk[channel] = users;
    }
    vector<string> channelTlkVect; 
    channelTlkVect = userTalk[username];
    channelTlkVect.insert(channelTlkVect.begin(), channel);
    userTalk[username] = channelTlkVect;
    return 0;
}

int leave_request(struct request_leave *leave) {
    string username, channel; 
    struct sockaddr_in address;
    //struct sockaddr_in addr;
    multimap<string, struct sockaddr_in>::iterator userIter;
    map<string,vector<pair<string,struct sockaddr_in> > >::iterator vectIter;
    vector<pair<string,struct sockaddr_in> > vect;
    
    cout << username << ": recv Request Leave" << endl;    
    
    username = get_current_user();
    //addr   = address_struct();
    channel  = (string)(leave->req_channel);
    userIter = user2addr.find(username);
    address  = userIter->second;
    vectIter = channelTalk.find(channel);
    vect = vectIter->second;
    for(int i = 0; i < vect.size(); i++) {
        if(vect[i].first == username) {
            if(check_equal(vect[i].second, address) == 0) {
                vect.erase(vect.begin() + i);
            } 
        }
    }
    
    channelTalk.erase(vectIter);
    if(vect.size() != 0) {
        channelTalk.insert(pair<string,vector<pair<string,struct sockaddr_in> > >(channel, vect));
        return 0;
    } else {
        for(int i = 0; i < channels.size(); i++) {
            if(channels[i] == channel) {
                channels.erase(channels.begin() + i);
            }
        }
    }
    vector<string> channelTalkVect;
    channelTalkVect = userTalk[username];
    for(int i = 0; i < channelTalkVect.size(); i++) {
        if(channelTalkVect[i] == channel) {
            channelTalkVect.erase(channelTalkVect.begin() + i);
        }
    }
    
    userTalk[username] = channelTalkVect;
    return 0;
}

int list_request() {
    int num, result;
    string username; 
    struct sockaddr_in address; 
    struct text_list *message;
    multimap<string, struct sockaddr_in>::iterator userIter; 
 
	num = channels.size();
    username = get_current_user();
    userIter = user2addr.find(username);
    address  = userIter->second;
    message  = (struct text_list *)malloc((sizeof(struct text_list) + (num * sizeof(struct channel_info))));
    message->txt_type = TXT_LIST;
    message->txt_nchannels = num;

    cout << username << ": recv Request List" << endl;    
    
    for (int i = 0; i < channels.size(); i++) {
        strcpy(((message->txt_channels)+i)->ch_channel, channels[i].c_str());
    }
    
    int res = sendto(sockfd, message, (sizeof(struct text_list)+(num * sizeof(struct channel_info))), 0, (struct sockaddr *)&address, sizeof(struct sockaddr));
    if (res < 0) {
        return -1;
    }
    free(message);
    return 0;
}

int who_request(struct request_who *who) {
    int num, result;
    string username, channel, ip, source; 
    struct text_who *message;
    struct sockaddr_in address;
    multimap<string, struct sockaddr_in>::iterator userIter;
    map<string,vector<pair<string,struct sockaddr_in> > >::iterator vectIter;
    vector<pair<string,struct sockaddr_in> > vect;
    
    username = get_current_user();
    channel  = (string)(who->req_channel);
   	userIter = user2addr.find(username);
    vectIter = channelTalk.find(channel);
    num      = (vectIter->second).size();
    vect     = vectIter->second;
    address  = userIter->second;
    message = (struct text_who *)malloc(sizeof(struct text_who) + (num * sizeof(struct user_info)));
    message->txt_type= TXT_WHO;
    message->txt_nusernames = num;
    strcpy(message->txt_channel, channel.c_str());

    cout << username << ": recv Request Who" << endl;
    
    for (int i = 0; i < vect.size(); i++) {
        strcpy(((message->txt_users) + i)->us_username, vect[i].first.c_str());
    }
   
    result = sendto(sockfd, message, (sizeof(struct text_who) + (num * sizeof(struct user_info))), 0, (struct sockaddr *)&address, sizeof(struct sockaddr));
    if (result < -1) {
        return -1;
    }
    free(message);
    return 0;
}

int say_request(struct request_say *say) {
    string channel, message, username;
    //struct sockaddr_in fromAddr = address_struct();
    map<string,vector<pair<string,struct sockaddr_in> > >::iterator iter; 
    vector<pair<string,struct sockaddr_in> > temp;
    
    channel  = say->req_channel;
	message  = say->req_text;
    username = get_current_user();
    iter = channelTalk.find(channel);
   
    if(iter == channelTalk.end()) {
        return -1; 
    }
    
    cout << username << ": recv Request Say" << endl;    
    temp = iter->second;
    
    for(int i = 0; i < temp.size(); i++) {
        int result;
        struct sockaddr_in address;
        struct text_say *data;
        
        address = temp[i].second;
        data = (struct text_say*) malloc(sizeof(struct text_say) + sizeof(message));
        data->txt_type = TXT_SAY;
        
        strncpy(data->txt_username, username.c_str(), USERNAME_MAX);
        strncpy(data->txt_text, message.c_str(), SAY_MAX);
        strncpy(data->txt_channel, channel.c_str(), CHANNEL_MAX);
        
        result = sendto(sockfd, data, (sizeof(data)), 0, (struct sockaddr*)&address, sizeof(struct sockaddr*));
        if (result == -1) {
            //error
        }
        free(data);
    }
    temp.clear();
    return 0;
}

int handle_input(struct request *req, int rec) {
    int result, host; 
    ssize_t bytes;
    struct sockaddr_in fromClient;
    
    result = 0;
    host   = 0;
    host = ntohl(req->req_type);
    
    if(host > 6 || host < 0) {
       host = req->req_type;
    }
    
    if(host != 0) {
        if(is_valid() == -1) {
            cout << "invalid address\n";
            return -1;
        } 
    }
    
if (host == REQ_LOGIN) {
		if (sizeof(struct request_login) == rec) {
			result = login_request((struct request_login *) req);
		} else {
			printf("Login request failed.\n");
		}
	} else if (host == REQ_LOGOUT) {
		if (sizeof(struct request_logout) == rec) {
			result = logout_request();
		} else {
			printf("Logout request failed.\n");
		}
	} else if (host == REQ_JOIN) {
		if (sizeof(struct request_join) == rec) {
			result = join_request((struct request_join *) req);
		} else {
			printf("Join request failed.\n");
		}
	} else if (host == REQ_LEAVE) {
		if (sizeof(struct request_leave) == rec) {
			result = leave_request((struct request_leave *) req);
		} else {
			printf("Leave request failed.\n");
		}
	} else if (host == REQ_SAY) {
		if (sizeof(struct request_say) == rec) {
			result = say_request((struct request_say *) req);
		} else {
			printf("Say request failed.\n");
		}
	} else if (host == REQ_LIST) {
		if (sizeof(struct request_list) == rec) {
			result = list_request();
		} else {
			printf("List request failed.\n");
		}
	} else if (host == REQ_WHO) {
		if (sizeof(struct request_who) == rec) {
			result =  who_request((struct request_who *) req);
		} else {
			error_request(fromClient, "** Unknown command :(.\n");
		}
	}
    return result;
}

int is_valid() {
    struct sockaddr_in *address;
    multimap<string,struct sockaddr_in>::iterator iter;
    
    address = (struct sockaddr_in*)&server;
    for(iter = user2addr.begin(); iter != user2addr.end(); iter++) {
        if(check_equal(iter->second, *address) == 0) {
            return 0;
        }
    }
    return -1;
}

int check_equal(struct sockaddr_in addr1, struct sockaddr_in addr2) {
    char *address1, *address2; 
    string string1, string2;
    address1 = (char*)malloc(sizeof(char)*BUFFER);
    address2 = (char*)malloc(sizeof(char)*BUFFER);
    inet_ntop(AF_INET, &(addr1.sin_addr), address1, BUFFER);
    inet_ntop(AF_INET, &(addr2.sin_addr), address2, BUFFER);
    string1 = address1;
    string2 = address2;
    if(string1 == string2) {
        int port1, port2;
        port1 = addr1.sin_port;
        port2 = addr2.sin_port;
        if(port1 == port2) {
            return 0;
        }
    }
    return -1;
}


void error_request(struct sockaddr_in sock, string msg) {
	ssize_t bytes;
	struct text_error error;
	struct sockaddr_in ssock;
	
	error.txt_type = TXT_ERROR;
	
	strcpy(error.txt_error, msg.c_str());
	ssock = sock;
	
	bytes = sendto(sockfd, &error, sizeof error, 0, (struct sockaddr *)&ssock, sizeof ssock);
	
	if (bytes < 0) {
		perror("Message failed.\n");
	}
}

string get_current_user() { 
    struct sockaddr_in *address;
    string temp;
    multimap<string,struct sockaddr_in>::iterator iter;
    
    address = (struct sockaddr_in*)&server;
    temp = "";     
    
    for(iter = user2addr.begin(); iter != user2addr.end(); iter++) {
        if(check_equal(iter->second, *address) == 0) {
            temp = iter->first;
        }
    }
    return temp;
}

struct sockaddr_in address_struct() {
    struct sockaddr_in *address;
    address = (struct sockaddr_in*)&server;
    return *address;
}

