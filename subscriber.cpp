#include "client_server.h"

#define STDIN_POLLFD fds[0]
#define TCP_POLLFD fds[1]

int main(int argc, char* argv[]) {
	int rc;
	sockaddr_in server_addr;
	pollfd fds[2];
	char buffer[1600];
	//used for receiving tcp messages
	char recv_buffer[1600];
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	TCP_POLLFD.events = POLLIN;
	STDIN_POLLFD.fd = STDIN_FILENO;
	STDIN_POLLFD.events = POLLIN;
	
	if(!inet_aton(argv[2], &server_addr.sin_addr)) {
		perror("Wrong ip address given as parameter.\n");
		exit(-1);
	}

	server_addr.sin_port = htons(atoi(argv[3]));
	server_addr.sin_family = PF_INET;

	//init connection

	TCP_POLLFD.fd = socket(AF_INET, SOCK_STREAM, 0);
	if(TCP_POLLFD.fd < 0) {
		perror("Could not open the socket.\n");
		exit(errno);
	}

	rc = 1;
	rc = setsockopt(TCP_POLLFD.fd, IPPROTO_TCP, TCP_NODELAY, &rc, sizeof(int));
	if(rc < 0) {
		perror("Could not deactivate Nagle's algorithm.\n");
		exit(errno);
	}

	rc = connect(TCP_POLLFD.fd, (sockaddr*)&server_addr, sizeof(sockaddr_in));
	if(rc < 0) {
		perror("Could not connect to the server.\n");
		exit(errno);
	}

	//sending msg with ID

	buffer[0] = 2 + strlen(argv[1]);
	buffer[1] = ID_MSG;
	strcpy(buffer + 2, argv[1]);

	rc = send(TCP_POLLFD.fd, buffer, buffer[0] + 1, 0);

	if(rc < 0) {
		perror("Could not send the ID message");
		exit(errno);
	}

	//begin of communications
	while(1) {
		sub_message send_message;
		server_message recv_message;
		string command;
		string word;

		send_message.id.assign(argv[1]);

		rc = poll(fds, 2, -1);

		if(rc < 0) {
			perror("An error occured while polling through received messages.\n");
			continue;
		}

		//input from user
		if(STDIN_POLLFD.revents == POLLIN) {
			getline(cin, command);
			stringstream c(command);

			if(!getline(c, word, ' '))
				continue;

			if(!word.compare("subscribe")) {

				if(!getline(c, word, ' ')) {
					cerr<<"Not enough parameters!\n";
					continue;
				}
					
				if(word.length() > TOPIC_LENGTH) {
					cerr<<"Wrong topic size!\n";
					continue;
				}
				
				send_message.topic.assign(word);

				if(!getline(c, word, ' ')) {
					cerr<<"Not enough parameters!\n";
					continue;
				}

				if(!word.compare("0")) {
					send_message.SF = 0;
				}
				else if(!word.compare("1")) {
					send_message.SF = 1;
				}
				else {
					cerr<<"Wrong SF given!\n";
					continue;
				}

				send_message.length = send_message.id.length() + send_message.topic.length() + 4;

				buffer[1] = SUB_MSG;
				strcpy(buffer + 2, send_message.id.data());
				strcpy(buffer + 3 + send_message.id.length(), send_message.topic.data());
				buffer[4 + send_message.id.length() + send_message.topic.length()] = send_message.SF;
				buffer[0] = send_message.length;
				rc = send(TCP_POLLFD.fd, buffer,buffer[0] + 1, 0);

				if(rc < 0) {
					perror("Could not send the subscribe message.\n");
					continue;
				}

				cout<<"Subscribed to topic.\n";

			}
			else if(!word.compare("unsubscribe")) {
				if(!getline(c, word, ' '))
					cerr<<"Not enough parameters!\n";

				if(word.length() > TOPIC_LENGTH) {
					cerr<<"Wrong topic size!\n";
					continue;
				}

				send_message.topic.assign(word);

				if(getline(c, word, ' '))
					cerr<<"Too many parameters!\n";

				send_message.length = send_message.id.length() + send_message.topic.length() + 3;

				buffer[1] = UNSUB_MSG;
				strcpy(buffer + 2, send_message.id.data());
				strcpy(buffer + send_message.id.length() + 3, send_message.topic.data());
				buffer[0] = send_message.length;
				rc = send(TCP_POLLFD.fd, buffer, buffer[0] + 1, 0);

				if(rc < 0) {
					perror("Could not send the unsubscribe message.\n");
					continue;
				}

				cout<<"Unsubscribed from topic.\n";
			}
			else if(!word.compare("exit")) {
				if(getline(c, word, ' '))
					cerr<<"Too many parameters!\n";

				shutdown(TCP_POLLFD.fd, SHUT_RDWR);
				close(TCP_POLLFD.fd);
				break;
			}
			else {
				cerr<<"Wrong command!\n";
			}
		}
		
		//message from server
		if(TCP_POLLFD.revents == POLLIN) {
			rc = read_n_bytes(TCP_POLLFD.fd, recv_buffer, buffer, 2);

			if(rc < 0) {
				perror("Could not receive message from server.\n");
				exit(errno);
			}

			if(!rc) {
				shutdown(TCP_POLLFD.fd, SHUT_RDWR);
				close(TCP_POLLFD.fd);
				break;
			}

			rc = read_n_bytes(TCP_POLLFD.fd, recv_buffer, buffer, ntohs(((uint16_t*)buffer)[0]));

			if(rc < 0) {
				perror("Could not receive message from server.\n");
				exit(errno);
			}

			if(!rc) {
				shutdown(TCP_POLLFD.fd, SHUT_RDWR);
				close(TCP_POLLFD.fd);
				break;
			}

			recv_message.type = buffer[0];

			//error or normal case
			switch (recv_message.type)
			{
			case USED_ID:
				perror("ID already used!\n");
				exit(-1);
			case NO_ID_GIVEN:
				perror("ID didn't reach the server.\n");
				exit(-1);	
			default:
				memcpy(&recv_message.sender_addr, buffer + 1, sizeof(sockaddr_in));
				strcpy(recv_message.topic, buffer + sizeof(sockaddr_in) + 1);
				memcpy(recv_message.data, buffer + sizeof(sockaddr_in) + strlen(recv_message.topic)
					+ 2, rc - sizeof(sockaddr_in) - strlen(recv_message.topic) - 2);
			}

			//differential output
			cout<<inet_ntoa(recv_message.sender_addr.sin_addr)<<":"
				<<ntohs(recv_message.sender_addr.sin_port)<<" - "<<recv_message.topic;
			cout<<fixed;
			switch (recv_message.type) {
			case 0:
				cout<<" - INT - "<<SIGN(*(uint8_t*)recv_message.data)
					<<ntohl(*(uint32_t*)(recv_message.data + 1))<<'\n';
				break;
			case 1:
				cout<<" - SHORT_REAL - "<<setprecision(2)
				<<ntohs(*(uint16_t*)recv_message.data) / 100.0<<'\n';
				break;
			case 2:
				cout<<" - FLOAT - "<<setprecision(6)
					<<SIGN(*(uint8_t*)recv_message.data)
					<<ntohl(*(uint32_t*)(recv_message.data + 1))
					* pow(10, -*(uint8_t*)(recv_message.data + 1 + sizeof(uint32_t)))<<'\n';
				break;
			case 3:
				cout<<" - STRING - "<<recv_message.data<<'\n';
				break;
			}
		}
	}
	
	return 0;
}