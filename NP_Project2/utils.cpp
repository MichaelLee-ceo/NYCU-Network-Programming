#include <string>
#include <regex>
#include <vector>
#include <cstring>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
using namespace std;

void show_cmd(vector<string> cmd){
	cout << "\nsplited command: ";
	for (int i = 0; i < cmd.size(); i++)
		cout << '[' << cmd[i] << ']' << ' ';
	cout << endl;
}

void show_queuing_cmd(vector<string> q){
	cout << "####On going command###" << endl;
	for (int k = 0; k < q.size(); k++)
		cout << "[" << k << "] " << q[k] << endl;
	cout << "########################\n" << endl;
}


void setenv_(string var, string value){
	setenv(var.c_str(), value.c_str(), 1);
}

void printenv_(string var){
	if (const char* env_p = getenv(var.c_str())){
		cout << env_p << endl;
	}
}

string clear_front_end_blank(string cmd){
	int start_pos = 0; 
	int end_pos = cmd.size() - 1;

	while (cmd[start_pos] == ' '){
		start_pos++;
	}
	while (cmd[end_pos] == ' '){
		end_pos--;
	}
	return cmd.substr(start_pos, end_pos-start_pos+1);
}

vector<string> parser(string command, string delimiter){
	vector<string> cmd_list;
	size_t pos1;
	string buffer;
	bool is_num;

	/* split command with delimiter */
	while ((pos1 = command.find_first_of(delimiter)) != string::npos){
		is_num = false;
		while (isdigit(command[pos1+1]) && delimiter != " "){
			pos1++;
			is_num = true;
		}

		if (isdigit(command[pos1])){
			buffer = command.substr(0, ++pos1);
			command.erase(0, pos1);
		}
		else{
			buffer = command.substr(0, pos1);
			command.erase(0, pos1 + delimiter.length());
		}

		buffer = clear_front_end_blank(buffer);
		if (buffer != "")
			cmd_list.push_back(buffer);
		
		// cout << "[" << buffer << "], pos1: " << pos1 << ", delimiter.length(): " << delimiter.length() << endl;
	}
	if (command != ""){
		command = clear_front_end_blank(command);
		cmd_list.push_back(command);
	}

	return cmd_list;
}

vector<string> split_number_pipe(string command){
	vector<string> cmd_list;
	regex reg(".*?([\\|\\!]{1}[0-9]+)");
	smatch match;

	while (regex_search(command, match, reg)){
		cmd_list.push_back(match[0].str());
		command = match.suffix().str();
	}

	if (command != "")
		cmd_list.push_back(command);
	return cmd_list;
}


bool built_in(string command, map<string, string> &user_envp){
	vector<string> command_string = parser(command, " ");
	if (command_string[0] == "setenv"){
		user_envp[command_string[1]] = command_string[2];
		// setenv_(command_string[1], command_string[2]);
	} else if (command_string[0] == "printenv"){
		cout << user_envp.find(command_string[1])->second << endl;
		// printenv_(command_string[1]);
	} else if (command_string[0] == "exit"){
		exit(0);
	} else
		return false;
	return true;
}


/* execute command: cat test.html */
void execute(string command, map<string, string> &user_envp){
	vector<string> command_string = parser(command, " ");
	int arg_len = command_string.size();
	char** argv = (char**) malloc((arg_len + 1) * sizeof(char*));

	for (int i = 0; i < arg_len; i++){
		argv[i] = (char*) command_string[i].c_str();
	}
	argv[arg_len] = NULL;

	for (map<string, string>::iterator it = user_envp.begin(); it != user_envp.end(); it++){
		setenv_(it->first, it->second);
	}

	if (execvp(argv[0], argv) == -1){
		string err_msg = "Unknown command: [" + command_string[0] + "].\n";
		write(STDERR_FILENO, err_msg.c_str(), err_msg.size());
		exit(0);
	}
}

vector<string> userpipe_parser(string command){
	vector<string> cmd_list;
	string id, tmp;
	regex reg_in(".*?(\\s<[0-9]+)");
	regex reg_out(".*?(\\s>[0-9]+)");

    smatch match_in;
    smatch match_out;

    /* 先找 user pipe 'in' */
    if (regex_search(command, match_in, reg_in)){
    	id = regex_replace(match_in[1].str(), regex("\\s\\<"), "");		/* 把 id 取出來 */
    	command = regex_replace(command, regex(match_in[1].str()), "");	/* 把 cmd 取出來 */
    	cmd_list.push_back(id);
    }

    if (regex_search(command, match_out, reg_out)){
    	id = regex_replace(match_out[1].str(), regex("\\s\\>"), "");		/* 把 id 取出來 */
		command = regex_replace(command, regex(match_out[1].str()), "");	/* 把 cmd 取出來 */
		cmd_list.push_back(id);
		// cout << "[>] cmd: " << command << ", " << id << endl;
	}

	cmd_list.insert(cmd_list.begin(), command);

    return cmd_list;
}

int* store_std(){
	int *std = new int[3];
	std[0] = dup(STDIN_FILENO);
	std[1] = dup(STDOUT_FILENO);
	std[2] = dup(STDERR_FILENO);
	return std;
}

void restore_std(int *std){
	dup2(std[0], STDIN_FILENO);
	dup2(std[1], STDOUT_FILENO);
	dup2(std[2], STDERR_FILENO);
	// close(std[0]);
	// close(std[1]);
	// close(std[2]);
	// delete[] std;
}