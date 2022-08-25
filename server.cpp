#include "client_server.h"

#define STDIN_POLLFD fds[0]
#define UDP_SERVER_POLLFD fds[1]
#define TCP_SERVER_POLLFD fds[2]

int main(int argc, char* argv[]) {
	int rc;
	//used for receiving tcp messages
	char tcp_recv_buffer[1600];
	char buffer[1600];
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	//<topic, <userId, SF>>
	unordered_map<string, unordered_map<string, bool>> users_by_topic;
	//<userId, (UserData, messages)>
	unordered_map<string, pair<user_data, queue<server_message>>> users_by_id;
	//<socked_fd, userId>
	unordered_map<int, string> users_by_socket;

	//all socket descriptors
	vector<struct pollfd> fds;
	fds.resize(3);

	//initialising the server socket descriptors
	STDIN_POLLFD.fd = STDIN_FILENO;
	STDIN_POLLFD.events = POLLIN;
	UDP_SERVER_POLLFD.events = POLLIN;
	TCP_SERVER_POLLFD.events = POLLIN;

	//initialising the address for receiving connections on tcp
	sockaddr_in recv_addrs;
	recv_addrs.sin_port = htons(atoi(argv[1]));
	recv_addrs.sin_addr.s_addr = INADDR_ANY;
	recv_addrs.sin_family = PF_INET;

	//udp connection init

	UDP_SERVER_POLLFD.fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(UDP_SERVER_POLLFD.fd < 0) {
		perror("Could not open a socket for udp connections.\n");
		exit(errno);
	}

	rc = bind(UDP_SERVER_POLLFD.fd, (sockaddr*)&recv_addrs, sizeof(recv_addrs));
	if(rc < 0) {
		perror("Could not bind the udp socket to the given port.\n");
		exit(errno);
	}

	//tcp connection init
	
	TCP_SERVER_POLLFD.fd = socket(AF_INET, SOCK_STREAM, 0);
	if(TCP_SERVER_POLLFD.fd < 0) {
		perror("Could not open a socket for tcp connections.\n");
		exit(errno);
	}

	rc = 1;
	rc = setsockopt(TCP_SERVER_POLLFD.fd, IPPROTO_TCP, TCP_NODELAY, &rc, sizeof(int));
	if(rc < 0) {
		perror("Could not deactivate Nagle's algorithm.\n");
		exit(errno);
	}

	rc = bind(TCP_SERVER_POLLFD.fd, (sockaddr*)&recv_addrs, sizeof(recv_addrs));
	if(rc < 0) {
		perror("Could not bind the tcp socket to the given port.\n");
		exit(errno);
	}

	rc = listen(TCP_SERVER_POLLFD.fd, -1);
	if(listen < 0) {
		perror("Could not set the tcp socket to listen to connections.\n");
		exit(errno);
	}
 
	//start communications
	while(1) {
		rc = poll(fds.data(), fds.size(), -1);

		if(rc < 0) {
			perror("An error occured while polling through received messages.\n");
			continue;
		}

		//command from server admin
		if(STDIN_POLLFD.revents == POLLIN) {
			string command;
			getline(cin, command);
			if(!command.compare("exit")) {
				for(int i = 1; i < fds.size(); i++) {
					shutdown(fds[i].fd, SHUT_RDWR);
					close(fds[i].fd);
				}

				break;
			}
		}

		//udp message
		if(UDP_SERVER_POLLFD.revents == POLLIN) {
			server_message message;
			socklen_t addr_size = sizeof(sockaddr_in);

			rc = recvfrom(UDP_SERVER_POLLFD.fd, (void*)buffer, 1600, 0,
				(sockaddr*)&message.sender_addr, &addr_size);
			if(rc < 0) {
				perror("Error when receiving an UDP message.\n");
				continue;;
			}
			
			//copying the topic
			memcpy(message.topic, buffer, TOPIC_LENGTH);
			message.topic[TOPIC_LENGTH] = 0;
			int topic_length = strlen(message.topic);

			//copying the other fields
			message.type = buffer[TOPIC_LENGTH];
			memcpy(message.data, buffer + TOPIC_LENGTH + 1, rc - TOPIC_LENGTH - 1);
			message.data[rc - TOPIC_LENGTH - 1] = 0;//usefull for the 1500 chars string case
			message.length = 2 + sizeof(sockaddr_in) + topic_length +
				rc - TOPIC_LENGTH;

			if(users_by_topic.find(message.topic) == users_by_topic.end()) {
				pair<string, unordered_map<string, bool>> map_entry;
				map_entry.first = message.topic;
				continue;
			}

			//filling the buffer
			buffer[2] = message.type;
			memcpy(buffer + 3, &message.sender_addr, sizeof(message.sender_addr));
			strcpy(buffer + 3 + sizeof(message.sender_addr), message.topic);
			memcpy(buffer + sizeof(message.sender_addr) + topic_length + 4, message.data,
				rc - TOPIC_LENGTH);
			((uint16_t*)buffer)[0] = htons(message.length);
			
			//sending the messages or storing them
			for(auto user : users_by_topic.at(message.topic))
			{
				if(users_by_id.at(user.first).first.status == ONLINE) {
					rc = send(users_by_id.at(user.first).first.fd, buffer,
					message.length + 2, 0);

					if(rc < 0)
						perror("Could not forward the message to a subscriber!\n");
				}
				else if(users_by_id.at(user.first).first.status == OFFLINE && user.second == 1)
				{
					users_by_id.at(user.first).second.push(message);
				}
			}
		}
		
		//tcp connection requests
		if(TCP_SERVER_POLLFD.revents == POLLIN) {
			user_data new_user;
			socklen_t addr_len = sizeof(sockaddr_in);
			pollfd new_fd;

			new_user.fd = accept(TCP_SERVER_POLLFD.fd, (sockaddr*)&new_user.addr,
				&addr_len);
			
			if(new_user.fd < 0) {
				perror("A TCP connection request failed.\n");
				continue;
			}
			
			//get length of this message
			rc = read_n_bytes(new_user.fd, tcp_recv_buffer, buffer, 1);

			if(rc <= 0) {
				perror("Did not receive a message with user ID.\n");
				shutdown(new_user.fd, SHUT_RDWR);
				close(new_user.fd);
				continue;
			}

			rc = read_n_bytes(new_user.fd, tcp_recv_buffer, buffer, buffer[0]);

			if(rc <= 0) {
				perror("Did not receive a message with user ID.\n");
				shutdown(new_user.fd, SHUT_RDWR);
				close(new_user.fd);
				continue;
			}

			//sending error when no ID came
			if(buffer[0] != ID_MSG) {
				((uint16_t*)buffer)[0] = htons(1);
				buffer[2] = NO_ID_GIVEN;

				rc = send(new_user.fd, buffer, 3, 0);
				if(rc < 0)
					perror("Could not send the error message to the subscriber!\n");

				shutdown(new_user.fd, SHUT_RDWR);
				close(new_user.fd);
				continue;
			}

			//getting user data
			new_user.id.assign(buffer + sizeof(uint8_t));
			new_user.status = ONLINE;
			new_fd.fd = new_user.fd;
			new_fd.events = POLLIN;

			//ID already in database
			if(users_by_id.find(new_user.id) != users_by_id.end()) {
				//someone already connected with that ID
				if(users_by_id.at(new_user.id).first.status == ONLINE) {
					((uint16_t*)buffer)[0] = htons(1);
					buffer[2] = USED_ID;

					rc = send(new_user.fd, buffer, 3, 0);
					if(rc < 0)
						perror("Could not send the error message to the subscriber!\n");
					
					cout<<"Client "<<new_user.id<<" already connected.\n";
					shutdown(new_user.fd, SHUT_RDWR);
					close(new_user.fd);
				}
				//reconnection on this ID
				else if(users_by_id.at(new_user.id).first.status == OFFLINE)  {
					users_by_id.find(new_user.id)->second.first = new_user;

					for(server_message msg = users_by_id.find(new_user.id)->second.second.front();
						!users_by_id.find(new_user.id)->second.second.empty();
						users_by_id.find(new_user.id)->second.second.pop()) {

						buffer[2] = msg.type;
						memcpy(buffer + 3, &msg.sender_addr, sizeof(sockaddr_in));
						strcpy(buffer + sizeof(sockaddr_in) + 3, msg.topic);
						memcpy(buffer + sizeof(sockaddr_in) + strlen(msg.topic) + 4, msg.data,
							msg.length - sizeof(sockaddr_in) - strlen(msg.data) - 2);
						((uint16_t*)buffer)[0] = htons(msg.length);
						rc = send(new_user.fd, buffer, ntohs(((uint16_t*)buffer)[0]) + 2, 0);

						if(rc < 0) {
							perror("Could not forward the message to a subscriber!\n");
						}
					}

					cout<<"New client "<<new_user.id<<" connected from "<<inet_ntoa(
						new_user.addr.sin_addr)<<":"<<ntohs(new_user.addr.sin_port)<<".\n";
					
					//changing some user data
					fds.push_back(new_fd);
					pair<int, string> map_entry;
					map_entry.first = new_user.fd;
					map_entry.second = new_user.id;
					users_by_socket.insert(map_entry);
				}

				continue;
			}

				//if the ID isn't used, we need to init the data for it
				pair<user_data, queue<server_message>> user_pair;
				pair<string, pair<user_data, queue<server_message>>> map_entry1;
				user_pair.first = new_user;
				map_entry1.first = new_user.id;
				map_entry1.second = user_pair;
				users_by_id.insert(map_entry1);
				cout<<"New client "<<new_user.id<<" connected from "<<inet_ntoa(
					new_user.addr.sin_addr)<<":"<<ntohs(new_user.addr.sin_port)<<".\n";
				fds.push_back(new_fd);
				pair<int, string> map_entry2;
				map_entry2.first = new_user.fd;
				map_entry2.second = new_user.id;
				users_by_socket.insert(map_entry2);
		}

		//TCP messages
		for(int i = 3; i < fds.size(); i++) {
			if(fds[i].revents == POLLIN) {
				sub_message message;
				
				//reading length first
				rc = read_n_bytes(fds[i].fd, tcp_recv_buffer, buffer, 1);
				
				if(rc < 0) {
					perror("Could not receive message from client.\n");
					exit(errno);
				}

				if(!rc) {
					cout<<"Client "<<users_by_socket.at(fds[i].fd)<<" disconnected.\n";
					users_by_id.at(users_by_socket.at(fds[i].fd)).first.status = OFFLINE;
					users_by_socket.erase(fds[i].fd);
					shutdown(fds[i].fd, SHUT_RDWR);
					close(fds[i].fd);
					fds.erase(fds.begin() + i);
					continue;
				}

				rc = read_n_bytes(fds[i].fd, tcp_recv_buffer, buffer, buffer[0]);

				if(rc < 0) {
					perror("Could not receive message from client.\n");
					exit(errno);
				}

				if(!rc) {
					cout<<"Client "<<users_by_socket.at(fds[i].fd)<<" disconnected.\n";
					users_by_id.at(users_by_socket.at(fds[i].fd)).first.status = OFFLINE;
					users_by_socket.erase(fds[i].fd);
					shutdown(fds[i].fd, SHUT_RDWR);
					close(fds[i].fd);
					fds.erase(fds.begin() + i);
					continue;
				}

				message.type = buffer[0];

				//accepting only sub and unsub messages
				if(message.type > 2) {
					continue;
				}

				message.id.assign(buffer + 1);
				message.topic.assign(buffer + message.id.length() + 2);

				if(message.type == SUB_MSG) {
					message.SF =
						*(bool*)(buffer + message.id.length() + message.topic.length() + 3);

					//creating topic if not existent
					if(users_by_topic.find(message.topic) == users_by_topic.end()) {
						pair<string, unordered_map<string, bool>> map_entry1;
						pair<string, bool> map_entry2;
						map_entry2.first = message.id;
						map_entry2.second = message.SF;
						map_entry1.first = message.topic;
						map_entry1.second.insert(map_entry2);
						users_by_topic.insert(map_entry1);
					}
					else {
						pair<string, bool> map_entry;
						map_entry.first = message.id;
						map_entry.second = message.SF;
						users_by_topic.find(message.topic)->second.insert(map_entry);
					}
				}
				else if(message.type == UNSUB_MSG) {
					if(users_by_topic.find(message.topic) != users_by_topic.end())
					users_by_topic.find(message.topic)->second.erase(message.id);
				}
			}
		}
	}

	return 0;
}