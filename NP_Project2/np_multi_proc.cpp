#include <iostream>
#include <algorithm>		// find()
#include <regex>			// string regex
#include <string>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>			// system call
#include <fcntl.h>			// file constants (O_CREAT, O_RDWR, ..)
#include <arpa/inet.h>
#include <sys/types.h>		// typedef symbols (pid_t, mode_t)
#include <sys/stat.h>		// file mode
#include <sys/socket.h>
#include <sys/signal.h>
#include <wait.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include "utils.cpp"
#include "user_multi.cpp"
using namespace std;

#define MAXLEN 20000
#define SHMFILE "shmFile"

int shm_fd;
Users users;


void user_signal_handler(int sig){
	if (sig == SIGUSR1){
		for (int i = 0; i < MAXCLIENT; i++){
			if (users.userList[i].isUnicast){
				User* receiver = users.getUserById(users.userList[i].to_id);
				write(receiver->fd, users.userList[i].msg, strlen(users.userList[i].msg));
				
				users.userList[i].isUnicast = false;
				users.userList[i].to_id = 0;
				break;
			}
			else if (users.userList[i].isBroadCast){
				string tmp_msg = users.userList[i].msg;
				users.broadcast(-1, users.userList[i].msg);
				users.userList[i].isBroadCast = false;
				write(users.userList[i].fd, "% ", 2);
				break;
			}
			else if (users.userList[i].isPipeFrom){
				string tmp_msg = users.userList[i].pipefrom_msg;
				users.broadcast(-1, users.userList[i].pipefrom_msg);
				users.userList[i].isPipeFrom = false;
				// write(users.userList[i].fd, "% ", 2);
				break;
			}
			else if (users.userList[i].isPipeTo){
				string tmp_msg = users.userList[i].pipeto_msg;
				users.broadcast(-1, users.userList[i].pipeto_msg);
				users.userList[i].isPipeTo = false;
				write(users.userList[i].fd, "% ", 2);

				// string fifoName = "./user_pipe/fifos" + to_string(users.userList[i].id) + "_" + to_string(users.userList[i].userpipe_to);
				// cout << "Create fifoName: " << fifoName << endl;

				break;
			}
			else if (users.userList[i].isExit){
				// cout << "[-] Closing Fd: " << users.userList[i].fd << ", id: " << users.userList[i].id << ", name: " << users.userList[i].name << endl;
				close(users.userList[i].fd);

				string str_id = to_string(users.userList[i].id);
				string tmp_name = users.userList[i].name;
				string msg = "*** User '" + tmp_name + "' left. ***\r\n";
				users.removeUser(users.userList[i].fd);
				users.broadcast(-1, msg);
				break;
			}
		}
	}
	else if (sig == SIGUSR2){
		users.OpenFifoBySignal(getpid());
	}
	else if (sig == SIGCHLD){
		waitpid(-1, NULL, WNOHANG);
	}
}

// void user_openFIFO_handler(int sig){
// 	users.OpenFifoBySignal(getpid());
// }

void sig_exit(int sig){
	// cout << "[--- Receive SIGINT ---]" << endl;
	users.clearUserPipe();
	munmap(users.userList, sizeof(User) * (MAXCLIENT+1));
	close(shm_fd);
	shm_unlink(SHMFILE);
	exit(0);
}

// void sig_handler(int sig){
// 	waitpid(-1, NULL, WNOHANG);
// }

int main(int argc, char *argv[]){
	setenv_("PATH", "bin:.");
	signal(SIGCHLD, user_signal_handler);
	signal(SIGINT, sig_exit);
	signal(SIGUSR1, user_signal_handler);
	signal(SIGUSR2, user_signal_handler);

	struct sockaddr_in server;
	struct sockaddr_in client;
	int sockfd, newsockfd;
	int pid;

	char buf[MAXLEN+1];
	int n;

	int *stdptr = store_std();

	/* open the shared memory object */
	if ((shm_fd = shm_open(SHMFILE, O_CREAT | O_RDWR, 0777)) == -1){
		cerr << "Server can't not create shared memory: " << strerror(errno) << endl;
		exit(-1);
	}
	if (ftruncate(shm_fd, sizeof(User) * (MAXCLIENT+1)) == -1){
		cerr << "Server ftruncate error: " << strerror(errno) << endl;
		exit(-1);
	}
	users.userList = (User*)mmap(NULL, sizeof(User) * (MAXCLIENT+1), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

	/* create socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		cerr << "Server can't open stream socket: " << strerror(errno) << endl;
		exit(-1);
	}

	int option = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0){
		cerr << "setsockopt (SO_RESUEADDR) error: " << strerror(errno) << endl;
	}

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(atoi(argv[1]));

	if (bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0){
		cerr << "Server: can't bind local address: " << strerror(errno) << endl;
		exit(-1);
	}

	listen(sockfd, 5);

	while (true){
		cout << "*** Server waiting for connections ***" << endl;
		int c = sizeof(client);
		if ((newsockfd = accept(sockfd, (struct sockaddr *) &client, (socklen_t *) &c)) < 0){
			cerr << "Server: accept error: " << strerror(errno) << endl;
			exit(-1);
		}
		else{
			char ip_arr[INET_ADDRSTRLEN];
            string ip = inet_ntop(AF_INET, &client.sin_addr, ip_arr, INET_ADDRSTRLEN);
            string port = to_string(ntohs(client.sin_port));
			
			users.addUser(newsockfd, ip + ":" + port, getpid());
			users.welcome(newsockfd);

			cout << "[+] Connection accpeted" << endl; 
		}

		if ((pid = fork()) > 0){			/* parent process */
			// close(newsockfd);
		}
		else if (pid == 0){					/* child process */
			close(sockfd);
			User* user = users.getUserByFd(newsockfd);
			user->pid = getpid();

			int status;
			int ctr = 0;
			int general_pipes[2][2];
			string command;
			string curr_cmd;

			vector<string> cmd_split_by_numberpipe;
			vector<string> command_list;

			pid_t pid1;
			pid_t wpid;
			bool has_pipe, is_number_pipe;
			bool stderr_flag;

			int from_id;
			int to_id;
			vector<string> userpipe_cmd;
			bool userpipeFrom, userpipeTo;

			regex reg(".*?([\\|\\!][0-9]+)");
			regex reg_userpipe(".*?(\\s[<>][0-9]+)");
			regex reg_in(".*?(\\s<[0-9]+)");
			regex reg_out(".*?(\\s>[0-9]+)");

			NumberPipeStore numberPipe;
			
			map<string, string> client_envp;
			client_envp["PATH"] = "bin:.";

			while ((n = read(newsockfd, buf, MAXLEN)) > 0){
				command = buf;
				memset(buf, 0, sizeof(buf));

				command = regex_replace(command, regex("\\n|\\r"), "");

				restore_std(stdptr);

				if (command == ""){
					write(newsockfd, "% ", 2);
					continue;
				}

				if (command.find("exit") != string::npos){
					user->isExit = true;
					users.notify(user->ppid);
					break;
				}

				cout << "\ncmd:" << command << endl;
				users.showUser(newsockfd);

				dup2(newsockfd, STDOUT_FILENO);
				dup2(newsockfd, STDERR_FILENO);

				if (built_in(command, client_envp)){
					numberPipe.decrementNumberPipe();
					write(newsockfd, "% ", 2);
					continue;
				}

				if (command == "who"){
					numberPipe.decrementNumberPipe();
					users.who(newsockfd);
				}
				else if (command.find("tell") != string::npos){
					numberPipe.decrementNumberPipe();
					users.tell(newsockfd, command);
				}
				else if (command.find("yell") != string::npos){
					numberPipe.decrementNumberPipe();
					users.yell(newsockfd, command);
				}
				else if (command.find("name") != string::npos){
					numberPipe.decrementNumberPipe();
					users.name(newsockfd, command);
				}
				else{
					cmd_split_by_numberpipe = split_number_pipe(command);		// 把 command 按照 nunber pipe去做分行
					// show_cmd(cmd_split_by_numberpipe);

					for (int x = 0; x < cmd_split_by_numberpipe.size(); x++){		// 每次取出一行 command 執行
						numberPipe.decrementNumberPipe();
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
									users.pipeFromId(newsockfd, from_id, command);
									users.pipeToId(newsockfd, to_id, command);
									userpipeFrom = true;
									userpipeTo = true;
								}
								else if (regex_search(tmp, reg_in)){
									from_id = stoi(userpipe_cmd[1]);
									users.pipeFromId(newsockfd, from_id, command);
									userpipeFrom = true;
								}
								else{
									to_id = stoi(userpipe_cmd[1]);
									users.pipeToId(newsockfd, to_id, command);
									userpipeTo = true;
								}
								curr_cmd = userpipe_cmd[0];
								// cerr << "[from_id]: " << from_id << "[to_id]: " << to_id << endl;
							}

							/* 判斷有沒有 pipe，
							   如果是一般 pipe，就使用 general_pipe，否則使用 number pipe */
							is_number_pipe = false;
							if (has_pipe){
								if (regex_match(curr_cmd, reg)){		/* 代表是 number pipe */
									curr_cmd = numberPipe.storeNumberPipe(curr_cmd);
									is_number_pipe = true;
								}
								else if (i < num_process - 1){
									pipe(general_pipes[ctr]);
									// cout << "Parent create general_pipe [" << ctr << "]: " << general_pipes[ctr][0] << ", " << general_pipes[ctr][1] << endl;
								}
							}

							while ((pid1 = fork()) < 0){
								waitpid(-1, NULL, 0);
							}

							if (pid1 > 0){						// parent process
								if (i > 0){
									close(general_pipes[!ctr][0]);
									close(general_pipes[!ctr][1]);
									// cout << "Parent close general_pipe[" + to_string(!ctr) + "] " + to_string(general_pipes[!ctr][0]) + ", " + to_string(general_pipes[!ctr][1]) + "\n";
								}

								numberPipe.closeNumberPipe();
								// users.closeUserPipe(newsockfd, from_id);

								if (!is_number_pipe){
									ctr++;
									ctr %= 2;
								}
							}
							else if (pid1 == 0){					// child process
								if (userpipeFrom){
									users.apply_user_pipeFrom(newsockfd, from_id);
								}
								if (userpipeTo){
									users.apply_user_pipeTo(newsockfd, to_id);
								}


								if (i == 0){									// first process
									int read_from = numberPipe.read_from_pipe();
									int write_to = numberPipe.write_to_pipe();

									if (read_from != -1){
										// cerr << getpid() << " [" << curr_cmd << "] Read from numberPipe: " << read_from << " " << user->numberPipe.getPipeById(read_from)->pipes[0] << endl;
										numberPipe.apply_stdin(read_from);
									}

									if (!is_number_pipe){
										if (has_pipe){
											close(general_pipes[ctr][0]);
											dup2(general_pipes[ctr][1], STDOUT_FILENO);
											close(general_pipes[ctr][1]);
										}
									}
									else{
										// cerr << getpid() << " [" << curr_cmd << "] Write to numberPipe: " << write_to << " " << numberPipe.getPipeById(write_to)->pipes[1] << endl;
										numberPipe.apply_stdout(write_to);
									}
								}
								else if (i > 0 && i < num_process - 1){			// middle process
									close(general_pipes[!ctr][1]);
									dup2(general_pipes[!ctr][0], STDIN_FILENO);
									close(general_pipes[!ctr][0]);

									close(general_pipes[ctr][0]);
									dup2(general_pipes[ctr][1], STDOUT_FILENO);
									close(general_pipes[ctr][1]);
								}
								else{											// last process
									// cerr << getpid() << " [" << curr_cmd << "] Read from general_pipes: " << !ctr << " " << general_pipes[!ctr][0] << endl;
									close(general_pipes[!ctr][1]);
									dup2(general_pipes[!ctr][0], STDIN_FILENO);
									close(general_pipes[!ctr][0]);

									if (is_number_pipe){
										int write_to = numberPipe.write_to_pipe();
										// cerr << getpid() << " [" << curr_cmd << "] Write to numberPipe: " << write_to << " " << user->numberPipe.getPipeById(write_to)->pipes[1] << endl;
										numberPipe.apply_stdout(write_to);
									}
								}

								if (curr_cmd.find('>') != string::npos){														/* file redirection */
									vector<string> file_cmd = parser(curr_cmd, ">");
									curr_cmd = file_cmd[0];

									int ffd = open(file_cmd[1].c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);		/* create file for file redirection */
									dup2(ffd, STDOUT_FILENO);
									close(ffd);
								}

								// cerr << "Execute: " << curr_cmd << ", " << getpid() << endl;
								execute(curr_cmd, client_envp);
							}
							if (userpipeFrom){
								while ((wpid = wait(&status)) > 0);
							}
						}
					}

					/* numberpipe 就不等 child，userpipe_to 也不等 child*/
					if (!is_number_pipe || !userpipeTo){
						while ((wpid = wait(&status)) > 0);
					}

					if (!userpipeTo){
						write(newsockfd, "% ", 2);
					}
				}
			}
			close(newsockfd);
			restore_std(stdptr);
			shm_unlink(SHMFILE);

			cout << "[-] Connection disconnected" << endl;
			exit(0);
		}
		else{
			cerr << "Fork error: " << strerror(errno) << endl;
		}
	}
	users.clearUserPipe();
	munmap(users.userList, sizeof(User) * (MAXCLIENT+1));
	close(shm_fd);
	shm_unlink(SHMFILE);
	return 0;
}