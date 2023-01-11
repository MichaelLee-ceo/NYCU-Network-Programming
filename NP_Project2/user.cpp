#include <iostream>
#include <vector>
#include <set>
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
#include "numberpipe.cpp"
using namespace std;

string digits = "0123456789";
vector<string> chat_parser(string command){
	vector<string> cmd;
	int pos1 = command.find_first_of(digits);
	int pos2 = command.find_first_not_of(digits, pos1);

	cmd.push_back(command.substr(pos1, pos2));
	cmd.push_back(command.substr(pos2+1));
	return cmd;
}


set<int> avail_ids;
set<string> names;

struct User {
	User (int Fd, string Ip){
		id = 0;
		fd = Fd;
		ip = Ip;
		name = "(no name)";

		ctr = 0;
		from_null = true;
		to_null = true;
	};

	bool operator < (const User &rhs) const{
        return (id < rhs.id);
    }

	int id;
	int fd;
	map<string, string> envp;
	string ip;
	string name;

	bool from_null;
	bool to_null;

	int ctr;
	int general_pipes[2][2];
	NumberPipeStore numberPipe;
};

struct UserPipe{
	UserPipe(int from_id, int to_id){
		from = from_id;
		to = to_id;

		pipe(pipes);
	};

	int from;
	int to;
	int pipes[2];
};


class Users {
public:
	Users(){
		for (int i = 1; i < 31; i++){
			avail_ids.insert(i);
		}
	}

	void addUser(User u);
	void removeUser(User u);
	void name(int fd, string name);
	void who(int fd);
	void tell(int id, string message);
	void yell(int fd, string message);
	void broadcast(int fd, string message);
	void welcome(int fd);
	void left(int fd);

	void pipeFromId(int fd, int sender_id, string command);
	void pipeToId(int fd, int receiver_id, string command);

	void apply_user_pipeFrom(int fd, int from_id);
	void apply_user_pipeTo(int fd, int to_id);

	void closeUserPipe(int fd, int from_id);
	void cleanUserPipe(int fd, int from_id);

	bool isUserPipeExist(int from_id, int to_id);
	UserPipe* getUserPipe(int from_id, int to_fd);

	void showUser(int fd);

	User* getUserById(int id);
	User* getUserByFd(int fd);

	vector<User> userList;
	vector<UserPipe> userPipeList;
};

void Users::addUser(User u){				/* 新增 user 時，要給 id, name, envp */
	u.id = *(avail_ids.begin());
	u.envp.insert(make_pair("PATH", "bin:."));
	avail_ids.erase(avail_ids.begin());		/* 要把 id 從 id_pool 中拿掉 */
	userList.push_back(u);

	cout << "[+] Server add user, ID: " << u.id << ", Name: " << u.name << ", IP: " << u.ip << endl;
}

void Users::removeUser(User u){
	avail_ids.insert(u.id);				/* 把 ID 還給 ID pool */
	if (u.name != "(no name)"){			/* 把 name 從 Name pool 中刪除*/
		names.erase(names.find(u.name));
	}

	for (vector<User>::iterator it = userList.begin(); it != userList.end(); it++){
		if (it->id == u.id){
			userList.erase(it);
			break;
		}
	}

	/* 要把跟當前離開 user 有關的 pipe 關掉 */
	for (vector<UserPipe>::iterator it2 = userPipeList.begin(); it2 != userPipeList.end();){
		if (it2->to == u.id || it2->from == u.id){
			close(it2->pipes[0]);
			close(it2->pipes[1]);
			it2 = userPipeList.erase(it2);
		}
		else
			it2++;
	}

	cout << "[-] Server remove user, ID: " << u.id << ", Name: " << u.name << ", IP: " << u.ip << endl;
}

/*
	name <new name>
*/
void Users::name(int fd, string command){
	string name = command.substr(command.find_first_of(" ") + 1);
	User* from = getUserByFd(fd);

	if (names.find(name) == names.end()){
		if (from->name != "(no name)")
			names.erase(names.find(from->name));

		from->name = name;
		names.insert(name);
		string msg = "*** User from " + from->ip + " is named '" + from->name + "'. ***\r\n";
		broadcast(-1, msg);

		// cout << "[O] user rename , ID: " << from->id << ", Name: " << from->name << endl;
	}
	else{
		string msg = "*** User '" + name + "' already exists. ***\r\n";
		write(fd, msg.c_str(), msg.size());

		// cout << "[X] Server add user, ID: " << from->id << ", Name: " << from->name << ", IP: " << from->ip << endl;
	}

	getUserByFd(fd)->numberPipe.decrementNumberPipe();
	write(fd, "% ", 2);
}

void Users::who(int fd){
	sort(userList.begin(), userList.end());

	string msg = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\r\n";
	for (int i = 0; i < userList.size(); i++){
		msg += to_string(userList[i].id) + "\t" + userList[i].name + "\t" + userList[i].ip + "\t";
		
		if (fd == userList[i].fd){
			msg += "<-me";
		}
		msg += "\r\n";
	}

	getUserByFd(fd)->numberPipe.decrementNumberPipe();
	write(fd, msg.c_str(), msg.length());
	write(fd, "% ", 2);
}

/*
	[ tell <user_id> <message> ]
	tell 3 Hello World.
*/
void Users::tell(int fd, string command){
	vector<string> cmd = chat_parser(command);
	int id = stoi(cmd[0]);
	string payload = cmd[1];

	User* from = getUserByFd(fd);
	User* to = getUserById(id);

	string msg;
	if (to != NULL){		/* 代表 user id 存在 */
		msg = "*** " + from->name + " told you ***: " + payload + "\r\n";
		write(to->fd, msg.c_str(), msg.length());
	}
	else{					/* to == NULL，代表 user id 不存在 */
		msg = "*** Error: user #" + to_string(id) + " does not exist yet. ***\r\n";
		write(from->fd, msg.c_str(), msg.length());
	}

	getUserByFd(fd)->numberPipe.decrementNumberPipe();
	write(fd, "% ", 2);

	// cout << "[# tell] " << msg;
}

/*
	[ yell <message> ]
	yell Good morning everyone.
*/
void Users::yell(int fd, string command){
	string payload = command.substr(command.find_first_of(" ") + 1);
	User* from = getUserByFd(fd);

	string msg = "*** " + from->name + " yelled ***: " + payload + "\r\n";
	broadcast(-1, msg);

	getUserByFd(fd)->numberPipe.decrementNumberPipe();
	write(fd, "% ", 2);

	// cout << "[# yell] " << msg;
}


void Users::broadcast(int fd, string msg){
	for (int i = 0; i < userList.size(); i++){
		if (userList[i].fd != fd){
			write(userList[i].fd, msg.c_str(), msg.length());
		}
	}
}


User* Users::getUserById(int id){
	for (int i = 0; i < userList.size(); i++){
		if (userList[i].id == id)
			return &userList[i];
	}
	return NULL;
}

User* Users::getUserByFd(int fd){
	for (int i = 0; i < userList.size(); i++){
		if (userList[i].fd == fd)
			return &userList[i];
	}
	return NULL;
}


void Users::welcome(int fd){
	string msg = "****************************************\r\n"
				 "** Welcome to the information server. **\r\n"
				 "****************************************\r\n";
	write(fd, msg.c_str(), msg.length());
}


void Users::left(int fd){
	User* from = getUserByFd(fd);
	string msg = "*** User '" + from->name + "' left. ***\r\n";
	broadcast(fd, msg);
	removeUser(*from);
}



void Users::pipeFromId(int fd, int sender_id, string command){		/* 從 from_id pipe 回來 */
	User* sender = getUserById(sender_id);
	User* receiver = getUserByFd(fd);
	string msg;

	if (sender == NULL){
		receiver->from_null = true;
		msg = "*** Error: user #" + to_string(sender_id) + " does not exist yet. ***\r\n";
		write(fd, msg.c_str(), msg.length());
	}
	else if (isUserPipeExist(sender->id, receiver->id)){				/* userPipeList 有找到 from 跟 to 的時候，代表 pipe 存在 */
		receiver->from_null = false;
		msg = "*** " + receiver->name + " (#" + to_string(receiver->id) + ") just received from " + sender->name + " (#" + to_string(sender->id) + ") by '" + command + "' ***\r\n";
		broadcast(-1, msg);
	}
	else {
		receiver->from_null = true;
		msg = "*** Error: the pipe #" + to_string(sender->id) + "->#" + to_string(receiver->id) + " does not exist yet. ***\r\n";
		write(fd, msg.c_str(), msg.length());
	}
}


void Users::pipeToId(int fd, int receiver_id, string command){			/* pipe 到 to_id */
	User* sender = getUserByFd(fd);
	User* receiver = getUserById(receiver_id);
	string msg;

	if (receiver == NULL){
		sender->to_null = true;
		msg = "*** Error: user #" + to_string(receiver_id) + " does not exist yet. ***\r\n";
		write(fd, msg.c_str(), msg.length());
	}
	else if (!isUserPipeExist(sender->id, receiver->id)){				/*  如果 userPipeList 中 沒有 from->to 的 pipe 的話 (還沒有 pipe 給別人) */
		sender->to_null = false;
		UserPipe newUserPipe(sender->id, receiver->id);
		userPipeList.push_back(newUserPipe);

		msg = "*** " + sender->name + " (#" + to_string(sender->id) + ") just piped '" + command + "' to " + receiver->name + " (#" + to_string(receiver->id) + ") ***\r\n";
		broadcast(-1, msg);
	}
	else{
		sender->to_null = true;
		msg = "*** Error: the pipe #" + to_string(sender->id) + "->#" + to_string(receiver->id) + " already exists. ***\r\n";
		write(fd, msg.c_str(), msg.length());
	}
}



void Users::apply_user_pipeFrom(int fd, int from_id){
	User* sender = getUserById(from_id);
	User* receiver = getUserByFd(fd);

	if (receiver->from_null == false){			/* from_null == false 時，代表這個 pipe 存在 */
		UserPipe* userPipe = getUserPipe(sender->id, receiver->id);
		close(userPipe->pipes[1]);
		dup2(userPipe->pipes[0], STDIN_FILENO);
		close(userPipe->pipes[0]);
	}
	else{										/* from == -1 時，代表 from_user 不存在 */
		int null_fd = open("/dev/null", O_RDONLY);
		dup2(null_fd, STDIN_FILENO);
		close(null_fd);
		// cerr << "[*] '/dev/null/' -> stdin " << null_fd << endl;
	}
}

void Users::apply_user_pipeTo(int fd, int to_id){
	User* sender = getUserByFd(fd);
	User* receiver = getUserById(to_id);

	if (sender->to_null == false){				/* to_null == false 時，代表這個 pipe 存在 */
		UserPipe* userPipe = getUserPipe(sender->id, receiver->id);
		close(userPipe->pipes[0]);
		dup2(userPipe->pipes[1], STDOUT_FILENO);
		close(userPipe->pipes[1]);
	}
	else{										/* to == -1 時，代表 to_user 不存在 */
		int null_fd = open("/dev/null", O_WRONLY);
		dup2(null_fd, STDOUT_FILENO);
		close(null_fd);
		// cerr << "[*] stdout -> '/dev/null/'" << null_fd << endl;
	}
}


void Users::cleanUserPipe(int from_id, int to_id){
	User* sender = getUserById(from_id);
	User* receiver = getUserById(to_id);

	for (vector<UserPipe>::iterator it = userPipeList.begin(); it != userPipeList.end(); it++){
		if ((it->from == sender->id) && (it->to == receiver->id)){
			userPipeList.erase(it);
			break;
		}
	}
}

void Users::closeUserPipe(int fd, int from_id){
	User* sender = getUserById(from_id);
	User* receiver = getUserByFd(fd);

	if (sender != NULL){
		UserPipe* userPipe = getUserPipe(sender->id, receiver->id);
		if (userPipe != NULL){
			close(userPipe->pipes[0]);
			close(userPipe->pipes[1]);
			cleanUserPipe(sender->id, receiver->id);
		}
	}
}

bool Users::isUserPipeExist(int from_id, int to_id){
	for (int i = 0; i < userPipeList.size(); i++){
		if (userPipeList[i].from == from_id && userPipeList[i].to == to_id)
			return true;
	}
	return false;
}

UserPipe* Users::getUserPipe(int from_id, int to_id){
	for (int i = 0; i < userPipeList.size(); i++){
		if (userPipeList[i].from == from_id && userPipeList[i].to == to_id)
			return &userPipeList[i];
	}
	return NULL;
}


void Users::showUser(int fd){
	User* u = getUserByFd(fd);
	cout << "User id: [" << u->id << "], sockfd: " << fd << endl;
}