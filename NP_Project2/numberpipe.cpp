#include <iostream>
#include <algorithm>
#include <regex>
#include <stdlib.h>
#include <unistd.h>
using namespace std;

struct NumberPipe {
	NumberPipe(int Id, int round, bool err_flag){
		id = Id;
		remain_round = round;
		stderr = err_flag;
		pipe(pipes);
	};

	int remain_round;			/* 還要等幾行    */
	int pipes[2];				/* 自己的 pipe   */
	int id;
	bool stderr;
};

class NumberPipeStore {
public:
	void addNumberPipe(int waiting_round, bool err_flag);
	void decrementNumberPipe();
	string storeNumberPipe(string command);
	void closeNumberPipe();
	void cleanNumberPipe();
	int read_from_pipe();
	int write_to_pipe();
	NumberPipe* getPipeById(int id);
	void apply_stdin(int id);
	void apply_stdout(int id);
	void show();

	int ids = 0;
	vector<NumberPipe> pipeList;
};

void NumberPipeStore::addNumberPipe(int waiting_round, bool err_flag){
	// cout << "[+] Add numberPipe id: " << ids << ", waiting_round: " << waiting_round << ", stderr: " << err_flag << endl;
	NumberPipe newPipe(ids, waiting_round, err_flag);				/* 新增一個 NumberPipe 物件*/
	pipeList.push_back(newPipe);
	ids++;
}

void NumberPipeStore::decrementNumberPipe(){
	for (int i = 0; i < pipeList.size(); i++){
		pipeList[i].remain_round--;
	}
}

string NumberPipeStore::storeNumberPipe(string command){
	vector<string> cmd_list;
	regex reg(".*?(\\|[0-9]+)");
	regex reg2(".*?(\\![0-9]+)");
	smatch match;
	string round;
	bool is_stderr = false;

	if (regex_search(command, match, reg)){
		command = regex_replace(command, regex("\\" + match[1].str()), "");	/* 把 command 取出來 */
		round = regex_replace(match[1].str(), regex("\\|"), "");			/* 把 round 取出來   */
		// cout << "Match stdout: " << command << ", round: " << round << endl;
	}
	else if (regex_search(command, match, reg2)){
		command = regex_replace(command, regex("\\" + match[1].str()), "");
		round = regex_replace(match[1].str(), regex("\\!"), "");
		is_stderr = true;
		// cout << "Match stderr: " << command << ", round: " << round << endl;
	}

	addNumberPipe(stoi(round), is_stderr);

	return command;
}

void NumberPipeStore::closeNumberPipe(){
	for (int i = 0; i < pipeList.size(); i++){
		if (pipeList[i].remain_round == 0){
			close(pipeList[i].pipes[0]);
			close(pipeList[i].pipes[1]);
		}
	}
	cleanNumberPipe();
}

void NumberPipeStore::cleanNumberPipe(){
	while (pipeList.size() > 0){
		if ((pipeList.begin()->remain_round) <= 0){
			pipeList.erase(pipeList.begin());
		}
		else
			break;
	}
}

int NumberPipeStore::read_from_pipe(){
	int read_from = -1;
	for (int i = 0; i < pipeList.size(); i++){
		if (pipeList[i].remain_round == 0){
			return pipeList[i].id;
		}
	}
	return -1;
}

int NumberPipeStore::write_to_pipe(){
	int write_to = -1;
	for (int i = 0; i < pipeList.size(); i++){
		if (pipeList[i].remain_round == (*--pipeList.end()).remain_round){
			return pipeList[i].id;
		}
	}
	return -1;
}


void NumberPipeStore::show(){
	cout << "***** waiting_queue *****" << endl;
	for (int i = 0; i < pipeList.size(); i++){
		cout << pipeList[i].remain_round << "  ";
	}

	// cout << "\n------ pipe_ptr -------" << endl;
	// for (int i = 0; i < pipeList.size(); i++){
	// 	cout << pipeList[i].ptr << " ";
	// }
	cout << "\n-----------------------" << endl << endl;
}

NumberPipe* NumberPipeStore::getPipeById(int id){
	for (int i = 0; i < pipeList.size(); i++){
		if (pipeList[i].id == id){
			return &pipeList[i];
		}
	}
	return NULL;
}

void NumberPipeStore::apply_stdin(int id){
	NumberPipe* from = getPipeById(id);
	dup2(from->pipes[0], STDIN_FILENO);
	close(from->pipes[0]);
	close(from->pipes[1]);
}

void NumberPipeStore::apply_stdout(int id){
	NumberPipe* to = getPipeById(id);
	dup2(to->pipes[1], STDOUT_FILENO);
	if (to->stderr){
		dup2(to->pipes[1], STDERR_FILENO);
	}
	close(to->pipes[1]);
	close(to->pipes[0]);
}
