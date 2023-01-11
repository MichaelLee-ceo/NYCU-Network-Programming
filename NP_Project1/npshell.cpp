#include <iostream>
#include <algorithm>
#include <string>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include "utils.cpp"
#include "numberpipe.cpp"
using namespace std;

// string msg = "Middle dup pipe[" + to_string(i) + "] " + to_string(pipes[i][1]) + " to stdout\n";
// write(STDERR_FILENO, msg.c_str(), msg.size());

void sig_handler(int signum){
	 waitpid(-1, NULL, WNOHANG);
}


int main(int argc, char* argv[], char* envp[]){
	/* Initial path */
	setenv_("PATH", "bin:.");
	signal(SIGCHLD, sig_handler);

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

	regex reg(".*?([\\|\\!][0-9]+)");

	NumberPipeStore numberPipe;
	

	while (true){
		cout << "% ";
		getline(cin, command);

		if (command == ""){
			continue;
		}

		/* built-in commands */
		if (built_in(command)){
			numberPipe.decrementNumberPipe();
			continue;
		}

		cmd_split_by_numberpipe = split_number_pipe(command);		// 把 command 按照 nunber pipe去做分行
		// show_cmd(cmd_split_by_numberpipe);

		for (int x = 0; x < cmd_split_by_numberpipe.size(); x++){		// 每次取出一行 command 執行
			numberPipe.decrementNumberPipe();
			command_list = parser(cmd_split_by_numberpipe[x], "|!");
			has_pipe = cmd_split_by_numberpipe[x].find_first_of("|!") != string::npos ? true : false;

			int num_process = command_list.size();
			for (int i = 0; i < num_process; i++){
				curr_cmd = command_list[i];

				/* 判斷有沒有 pipe，
				   如果是一般 pipe，就使用 general_pipe，否則使用 number pipe */
				is_number_pipe = false;
				if (has_pipe){
					stderr_flag = curr_cmd.find_first_of("!") != string::npos ? true : false;

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

					if (!is_number_pipe){
						ctr++;
						ctr %= 2;
					}
				}
				else if (pid1 == 0){					// child process
					if (i == 0){									// first process
						int read_from = numberPipe.read_from_pipe();
						int write_to = numberPipe.write_to_pipe();

						if (read_from != -1){
							// cerr << getpid() << " [" << curr_cmd << "] Read from numberPipe: " << read_from << " " << numberPipe.getPipeById(read_from)->pipes[0] << endl;
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
							// cerr << getpid() << " [" << curr_cmd << "] Write to numberPipe: " << write_to << " " << numberPipe.getPipeById(write_to)->pipes[1] << endl;
							numberPipe.apply_stdout(write_to);
						}
					}

					if (curr_cmd.find('>') != string::npos){														/* file redirection */
						vector<string> file_cmd = parser(curr_cmd, ">");
						curr_cmd = file_cmd[0];

						int fd = open(file_cmd[1].c_str(), O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);		/* create file for file redirection */
						dup2(fd, STDOUT_FILENO);
						close(fd);
					}

					// cerr << getpid() << " execute: " << curr_cmd << endl;
					execute(curr_cmd);
				}
			}
		}
		if (!is_number_pipe){
			while ((wpid = wait(&status)) > 0);
		}
	}
	return 0;
}