#include <iostream>
#include <algorithm>		// find()
#include <regex>			// string regex
#include <string>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>			// system call
#include <fcntl.h>			// file constants (O_CREAT, O_RDWR, ..)
#include <arpa/inet.h>
#include <sys/types.h>		// typedef symbols (pid_t, mode_t)
#include <sys/stat.h>		// file mode
#include <sys/socket.h>
#include <sys/signal.h>
#include <wait.h>
#include "utils.cpp"
#include "user.cpp"
using namespace std;

#define MAXLEN 20000

void sig_handler(int sig){
	waitpid(-1, NULL, WNOHANG);
}

int main(int argc, char *argv[]){
	setenv_("PATH", "bin:.");
	signal(SIGCHLD, sig_handler);

	int status;
	string command;
	string curr_cmd;

	vector<string> cmd_split_by_numberpipe;
	vector<string> command_list;

	pid_t pid1;
	pid_t wpid;
	bool has_pipe, is_number_pipe, is_user_pipe;
	bool stderr_flag;

	regex reg(".*?([\\|\\!][0-9]+)");
	regex reg_userpipe(".*?(\\s[<>][0-9]+)");
	regex reg_in(".*?(\\s<[0-9]+)");
	regex reg_out(".*?(\\s>[0-9]+)");

	int *stdfd = store_std();

	struct sockaddr_in server;
	struct sockaddr_in client;
	int sockfd, newsockfd;
	int pid;

	char buf[MAXLEN+1];
	int n;

	fd_set rfds;
	fd_set afds;

	Users userList;

	int from_id;
	int to_id;
	vector<string> userpipe_cmd;
	bool userpipeFrom, userpipeTo;

	/* create socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		cerr << "Server can't open stream socket: " << strerror(errno) << endl;
		exit(-1);
	}

	int option = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0){
		cerr << "setsockopt (SO_REUSEADDR) error: " << strerror(errno) << endl;
	}

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(atoi(argv[1]));

	/* bind socket to well-known port */
	if (bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0){
		cerr << "Server can't bind local address: " << strerror(errno) << endl;
		exit(-1);
	}

	listen(sockfd, 5);

	int nfds = getdtablesize();

	FD_ZERO(&afds);
	FD_SET(sockfd, &afds);

	int *stdptr = store_std();

	while (true){
		cout << "*** Server waiting for connection ***" << endl;

		memcpy(&rfds, &afds, sizeof(rfds));

		/* 看當前有哪些 client 傳 request 過來 */
		if (select(nfds, &rfds, NULL, NULL, NULL) < 0){
			cerr << "select error: " << strerror(errno) << endl;
			continue;
		}

		if (FD_ISSET(sockfd, &rfds)){			// receive connection request
			int c = sizeof(client);
			if ((newsockfd = accept(sockfd, (struct sockaddr *) &client, (socklen_t *) &c)) < 0){
				cerr << "Server accept error: " << strerror(errno) << endl;
				continue;
			}
			else{
				FD_SET(newsockfd, &afds);

				char ip_arr[INET_ADDRSTRLEN];
	            string ip = inet_ntop(AF_INET, &client.sin_addr, ip_arr, INET_ADDRSTRLEN);
	            string port = to_string(ntohs(client.sin_port));
				string msg = "*** User '(no name)' entered from " + ip + ":" + port + ". ***\n";
				
				userList.addUser(User(newsockfd, ip + ":" + port));
				userList.welcome(newsockfd);
				userList.broadcast(-1, msg);
				write(newsockfd, "% ", 2);

				cout << "[+] Accepted: " << msg << endl;
			}
		}

		/* handle incoming requests from clients */
		for (int fd = 0; fd < nfds; fd++){
			if (fd != sockfd && FD_ISSET(fd, &rfds)){
				n = read(fd, buf, MAXLEN);
				if (n == 0){					/* 如果 n == 0，代表 client 結束連線 (EOF) */
					FD_CLR(fd, &afds);
					close(fd);
				}
				else{							/* 如果 n > 0，代表 client 有送 command 過來 */
					string command = buf;
					memset(buf, 0, sizeof(buf));

					command = regex_replace(command, regex("\\n|\\r"), "");

					restore_std(stdptr);

					if (command == ""){
						write(fd, "% ", 2);
						continue;
					}
					
					if (command.find("exit") != string::npos){
						userList.left(fd);
						FD_CLR(fd, &afds);
						close(fd);

						cout << "[-] Client disconnected: " << fd << endl;
						continue;
					}

					User* user = userList.getUserByFd(fd);
					cout << "\ncmd:" << command << endl;
					userList.showUser(fd);

					dup2(fd, STDOUT_FILENO);
					dup2(fd, STDERR_FILENO);

					if (built_in(command, user->envp)){
						user->numberPipe.decrementNumberPipe();
						write(fd, "% ", 2);
						continue;
					}

					/* 檢查是不是 chatting command (who, tell, yell, name) */
					if (command == "who"){
						userList.who(fd);
					}
					else if (command.find("tell") != string::npos){
						userList.tell(fd, command);
					}
					else if (command.find("yell") != string::npos){
						userList.yell(fd, command);
					}
					else if (command.find("name") != string::npos){
						userList.name(fd, command);
					}
					else{
						cmd_split_by_numberpipe = split_number_pipe(command);		// 把 command 按照 nunber pipe去做分行
						// show_cmd(cmd_split_by_numberpipe);

						for (int x = 0; x < cmd_split_by_numberpipe.size(); x++){		// 每次取出一行 command 執行
							user->numberPipe.decrementNumberPipe();
							command_list = parser(cmd_split_by_numberpipe[x], "|!");
							has_pipe = cmd_split_by_numberpipe[x].find_first_of("|!") != string::npos ? true : false;

							int num_process = command_list.size();
							for (int i = 0; i < num_process; i++){
								curr_cmd = command_list[i];

								userpipeFrom = false;
								userpipeTo = false;
								from_id = -1;
								to_id = -1;

								if (regex_search(curr_cmd, reg_userpipe)){
									string tmp = curr_cmd;
									userpipe_cmd = userpipe_parser(curr_cmd);			/* 回傳格式: [cmd, from_id, to_id] */

									if (regex_search(tmp, reg_in) && regex_search(tmp, reg_out)){		/* cat <2 >2 */
										from_id = stoi(userpipe_cmd[1]);
										to_id = stoi(userpipe_cmd[2]);
										userList.pipeFromId(fd, from_id, command);
										userList.pipeToId(fd, to_id, command);
										userpipeFrom = true;
										userpipeTo = true;
									}
									else if (regex_search(tmp, reg_in)){
										from_id = stoi(userpipe_cmd[1]);
										userList.pipeFromId(fd, from_id, command);
										userpipeFrom = true;
									}
									else{
										to_id = stoi(userpipe_cmd[1]);
										userList.pipeToId(fd, to_id, command);
										userpipeTo = true;
									}
									curr_cmd = userpipe_cmd[0];
								}

								/* 判斷有沒有 pipe，
								   如果是一般 pipe，就使用 general_pipe，否則使用 number pipe */
								is_number_pipe = false;
								if (has_pipe){
									if (regex_match(curr_cmd, reg)){		/* 代表是 number pipe */
										curr_cmd = user->numberPipe.storeNumberPipe(curr_cmd);
										is_number_pipe = true;
									}
									else if (i < num_process - 1){
										pipe(user->general_pipes[user->ctr]);
										// cout << "Parent create general_pipe [" << ctr << "]: " << general_pipes[ctr][0] << ", " << general_pipes[ctr][1] << endl;
									}
								}

								while ((pid1 = fork()) < 0){
									waitpid(-1, NULL, 0);
								}

								if (pid1 > 0){						// parent process
									if (i > 0){
										close(user->general_pipes[!user->ctr][0]);
										close(user->general_pipes[!user->ctr][1]);
										// cout << "Parent close general_pipe[" + to_string(!ctr) + "] " + to_string(general_pipes[!ctr][0]) + ", " + to_string(general_pipes[!ctr][1]) + "\n";
									}

									user->numberPipe.closeNumberPipe();
									userList.closeUserPipe(fd, from_id);

									if (!is_number_pipe){
										user->ctr++;
										user->ctr %= 2;
									}
								}
								else if (pid1 == 0){					// child process
									if (userpipeFrom){
										userList.apply_user_pipeFrom(fd, from_id);
									}
									if (userpipeTo){
										userList.apply_user_pipeTo(fd, to_id);
									}


									if (i == 0){									// first process
										int read_from = user->numberPipe.read_from_pipe();
										int write_to = user->numberPipe.write_to_pipe();

										if (read_from != -1){
											// cerr << getpid() << " [" << curr_cmd << "] Read from numberPipe: " << read_from << " " << user->numberPipe.getPipeById(read_from)->pipes[0] << endl;
											user->numberPipe.apply_stdin(read_from);
										}

										if (!is_number_pipe){
											if (has_pipe){
												close(user->general_pipes[user->ctr][0]);
												dup2(user->general_pipes[user->ctr][1], STDOUT_FILENO);
												close(user->general_pipes[user->ctr][1]);
											}
										}
										else{
											// cerr << getpid() << " [" << curr_cmd << "] Write to numberPipe: " << write_to << " " << numberPipe.getPipeById(write_to)->pipes[1] << endl;
											user->numberPipe.apply_stdout(write_to);
										}
									}
									else if (i > 0 && i < num_process - 1){			// middle process
										close(user->general_pipes[!user->ctr][1]);
										dup2(user->general_pipes[!user->ctr][0], STDIN_FILENO);
										close(user->general_pipes[!user->ctr][0]);

										close(user->general_pipes[user->ctr][0]);
										dup2(user->general_pipes[user->ctr][1], STDOUT_FILENO);
										close(user->general_pipes[user->ctr][1]);
									}
									else{											// last process
										// cerr << getpid() << " [" << curr_cmd << "] Read from general_pipes: " << !ctr << " " << general_pipes[!ctr][0] << endl;
										close(user->general_pipes[!user->ctr][1]);
										dup2(user->general_pipes[!user->ctr][0], STDIN_FILENO);
										close(user->general_pipes[!user->ctr][0]);

										if (is_number_pipe){
											int write_to = user->numberPipe.write_to_pipe();
											// cerr << getpid() << " [" << curr_cmd << "] Write to numberPipe: " << write_to << " " << user->numberPipe.getPipeById(write_to)->pipes[1] << endl;
											user->numberPipe.apply_stdout(write_to);
										}
									}

									if (curr_cmd.find('>') != string::npos){														/* file redirection */
										vector<string> file_cmd = parser(curr_cmd, ">");
										curr_cmd = file_cmd[0];

										int ffd = open(file_cmd[1].c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);		/* create file for file redirection */
										dup2(ffd, STDOUT_FILENO);
										close(ffd);
									}

									// cerr << "Execute: " << curr_cmd << endl;
									execute(curr_cmd, user->envp);
								}
								if (userpipeFrom){
									while ((wpid = wait(&status)) > 0);
								}
							}
						}
						if (!is_number_pipe || !userpipeTo){
							while ((wpid = wait(&status)) > 0);
						}
						write(fd, "% ", 2);
					}
					
				}
			}
		}
		restore_std(stdptr);
	}
	return 0;
}