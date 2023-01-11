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

#define MAXCLIENT 30

string baseFifoName = "./user_pipe/fifos";

string digits = "0123456789";
vector<string> chat_parser(string command){
	vector<string> cmd;
	int pos1 = command.find_first_of(digits);
	int pos2 = command.find_first_not_of(digits, pos1);

	cmd.push_back(command.substr(pos1, pos2));
	cmd.push_back(command.substr(pos2+1));
	return cmd;
}

struct UserFifo {					/* ex: 1->2 ,  /user_pipe/fifos1_2 */
	char fifoname[100] = {0};
	int readfd = -1;
};

struct User {
	int id = 0;						/* User 的 ID */
	int fd = 0;						/* User 的 socket fd */
	int pid = 0;
	int ppid = 0;					/* Parent 的 pid */
	int to_id = 0;
	int userpipe_to = 0;
	int readfd = 0;

	char ip[100] = {0};				/* User 的 ip:port */
	char name[100] = {0};			/* User 的名字 */
	char msg[1024] = {0};			/* User 要傳給別人或廣播的 message */
	char pipefrom_msg[1024] = {0};
	char pipeto_msg[1024] = {0};

	UserFifo fifoList[100] = {0};
	char writeFifoHere[100] = {0};
	char allfifo[100][100] = {0};
	
	bool from_null = true;
	bool to_null = true;
	bool isUnicast = false;
	bool isBroadCast = false;
	bool isPipeFrom = false;
	bool isPipeTo = false;
	bool isExit = false;
};


class Users {
public:
	Users(){
		;
	};

	void addUser(int fd, string Ip, pid_t ppid);
	void clearUser(User* u);
	void removeUser(int fd);
	void name(int fd, string name);
	void who(int fd);
	void tell(int id, string message);
	void yell(int fd, string message);
	void broadcast(int fd, string message);
	void welcome(int fd);

	void pipeFromId(int fd, int sender_id, string command);
	void pipeToId(int fd, int receiver_id, string command);

	void apply_user_pipeFrom(int fd, int from_id);
	void apply_user_pipeTo(int fd, int to_id);

	void createFIFO(int from_id, int to_id);
	bool isUserPipeExist(int from_id, int to_id);
	void clearUserPipe();
	void cleanFifoList(string fifo_name);
	void OpenFifoBySignal(int recv_pid);

	void showUser(int fd);
	void notify(int target_pid, bool openFiFo);

	User* getUserById(int id);
	User* getUserByFd(int fd);
	User* getUserByPid(int pid);

	User* userList;
};

void Users::createFIFO(int from_id, int to_id){
	string fifo_name = baseFifoName + to_string(from_id) + "_" + to_string(to_id);
	if ((mknod(fifo_name.c_str(), S_IFIFO | 0777, 0) < 0) && (errno != EEXIST)){
		// cerr << "Can't create FIFO: " << fifo_name << ", " << strerror(errno) << endl;
	}
	else{
		// cout << "[Create FIFO]: " << fifo_name << endl;
		
		/*  1 要 cat >2，就把 fifo name 寫到 2 底下的 share memory */
		User* receiver = getUserById(to_id);
		strcpy(receiver->writeFifoHere, fifo_name.c_str());

		/* 在 allfifo 裡面找到第一個空的放 */
		for (int i = 0; i < 100; i++){
			if (strlen(userList[MAXCLIENT].allfifo[i]) == 0){
				strcpy(userList[MAXCLIENT].allfifo[i], fifo_name.c_str());
				break;
			}
		}
	}
}

void Users::notify(int target_pid, bool openFiFo=false){
	if (!openFiFo){
		kill(target_pid, SIGUSR1);
	}
	else{
		kill(target_pid, SIGUSR2);
	}
}

/* 
	新增 user 之後，要把新的 user 寫進去 shared memory 
	1. 要先更新自己的 userList (更新當前的 user)
	2. 更新完之後，再把自己的 user 寫進去
*/
void Users::addUser(int fd, string Ip, pid_t ppid){				/* 新增 user 時，要給 id, name, envp */
	string defaultName = "(no name)";
	for (int i = 0; i < MAXCLIENT; i++){
		if (userList[i].id == 0){
			userList[i].fd = fd;
			userList[i].ppid = ppid;
			strcpy(userList[i].ip, Ip.c_str());
			strcpy(userList[i].name, defaultName.c_str());
			userList[i].id = i+1;

			cout << "[+] Server add user, [" << i << "] ID: " << userList[i].id << ", Name: " << userList[i].name << ", IP: " << userList[i].ip << ", ppid: " << userList[i].ppid << endl;
			break;
		}
	}
}

void Users::clearUser(User* u){
	u->id = 0;
	u->fd = 0;
	u->to_id = 0;
	u->pid = 0;
	u->ppid = 0;
	u->userpipe_to = 0;
	u->readfd = 0;
	memset(u->ip, 0, sizeof(u->ip));
	memset(u->name, 0, sizeof(u->name));
	memset(u->msg, 0, sizeof(u->msg));
	memset(u->pipefrom_msg, 0, sizeof(u->pipefrom_msg));
	memset(u->pipeto_msg, 0, sizeof(u->pipeto_msg));
	u->from_null = true;
	u->to_null = true;
	u->isBroadCast = false;
	u->isUnicast = false;
	u->isExit = false;
	u->isPipeFrom = false;
	u->isPipeTo = false;
}

void Users::removeUser(int fd){
	User* u = getUserByFd(fd);
	string user_id = to_string(u->id);

	/* 要把跟當前離開 user 有關的 fifo 關掉 */
	/* 還留在 每一個 user 的 fifoList 裡面的 fifo */
	for (int i = 0; i < MAXCLIENT; i++){
		for (int j = 0; j < 100; j++){
			string tmp_fname = userList[i].fifoList[j].fifoname;

			if (tmp_fname.find(user_id) != string::npos){
				unlink(userList[i].fifoList[j].fifoname);
				strcpy(userList[i].fifoList[j].fifoname, "");
				userList[i].fifoList[j].readfd = -1;
				cout << "[*] User remove from fifoList: " << tmp_fname << endl;
			}
		}
	}

	/* 從 global fifoList 中 unlink */
	for (int i = 0; i < 100; i++){
		if (strlen(userList[MAXCLIENT].allfifo[i]) != 0){
			string trash_fifo_name = userList[MAXCLIENT].allfifo[i];
			if (trash_fifo_name.find(user_id) != string::npos){
				unlink(userList[MAXCLIENT].allfifo[i]);
				strcpy(userList[MAXCLIENT].allfifo[i], "");
				cout << "[*] User remove from allfifo: " << trash_fifo_name << endl;
			}
		}
	}
	cout << "[-] Server remove user, ID: " << u->id << ", Name: " << u->name << ", IP: " << u->ip << endl;
	clearUser(u);
}

/*
	name <new name>
*/
void Users::name(int fd, string command){
	string name = command.substr(command.find_first_of(" ") + 1);
	User* from = getUserByFd(fd);

	bool isExist = false;
	for (int i = 0; i < MAXCLIENT; i++){
		if (userList[i].id != 0 && (strcmp(userList[i].name, name.c_str()) == 0)){
			isExist = true;
			break;
		}
	}
	
	if (!isExist){
		strcpy(from->name, name.c_str());

		string tmp_ip = from->ip;
		string tmp_name = from->name;
		string msg = "*** User from " + tmp_ip + " is named '" + tmp_name + "'. ***\r\n";
		
		strcpy(from->msg, msg.c_str());
		from->isBroadCast = true;
		notify(from->ppid);				/* send signal 給 server，跟他說有訊息要傳給別人*/
		while (from->isBroadCast){
			;
		}
	}
	else{
		string msg = "*** User '" + name + "' already exists. ***\r\n";
		write(fd, msg.c_str(), msg.size());
		write(fd, "% ", 2);
	}
	// write(fd, "% ", 2);
}

void Users::who(int fd){
	string msg = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\r\n";
	for (int i = 0; i < MAXCLIENT; i++){
		if (userList[i].id != 0){
			msg += to_string(userList[i].id) + "\t" + userList[i].name + "\t" + userList[i].ip + "\t";
		
			if (fd == userList[i].fd){
				msg += "<-me";
			}
			msg += "\r\n";
		}	
	}
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
		string tmp_name = from->name;
		msg = "*** " + tmp_name + " told you ***: " + payload + "\r\n";
		
		strcpy(from->msg, msg.c_str());
		from->to_id = id;
		from->isUnicast = true;
		notify(from->ppid);
		while (from->isUnicast){
			;
		}
	}
	else{					/* to == NULL，代表 user id 不存在 */
		msg = "*** Error: user #" + to_string(id) + " does not exist yet. ***\r\n";
		write(from->fd, msg.c_str(), msg.length());
	}
	write(fd, "% ", 2);
}

/*
	[ yell <message> ]
	yell Good morning everyone.
*/
void Users::yell(int fd, string command){
	string payload = command.substr(command.find_first_of(" ") + 1);
	User* from = getUserByFd(fd);
	string tmp_name = from->name;

	string msg = "*** " + tmp_name + " yelled ***: " + payload + "\r\n";
	strcpy(from->msg, msg.c_str());
	from->isBroadCast = true;

	notify(from->ppid);
	while (from->isBroadCast){
		;
	}
}


void Users::broadcast(int fd, string msg){
	for (int i = 0; i < MAXCLIENT; i++){
		if ((userList[i].fd != fd) && (userList[i].id != 0)){
			write(userList[i].fd, msg.c_str(), msg.length());
		}
	}
}


User* Users::getUserById(int id){
	for (int i = 0; i < MAXCLIENT; i++){
		if (userList[i].id == id)
			return &userList[i];
	}
	return NULL;
}

User* Users::getUserByFd(int fd){
	for (int i = 0; i < MAXCLIENT; i++){
		if (userList[i].fd == fd){
			return &userList[i];
		}
	}
	return NULL;
}

User* Users::getUserByPid(int pid){
	for (int i = 0; i < MAXCLIENT; i++){
		if (userList[i].pid == pid){
			return &userList[i];
		}
	}
	return NULL;
}


void Users::welcome(int fd){
	User* from = getUserByFd(fd);
	string msg = "****************************************\r\n"
				 "** Welcome to the information server. **\r\n"
				 "****************************************\r\n";
	write(fd, msg.c_str(), msg.length());

	string tmp_ip = from->ip;
	string bmsg = "*** User '(no name)' entered from " + tmp_ip + ". ***\n";
	
	strcpy(from->msg, bmsg.c_str());
	from->isBroadCast = true;
	notify(from->ppid);
	while (from->isBroadCast){
		;
	}
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
		sender->userpipe_to = 0;
		
		string tmp_sendname = sender->name;
		string tmp_recvname = receiver->name;
		msg = "*** " + tmp_recvname + " (#" + to_string(receiver->id) + ") just received from " + tmp_sendname + " (#" + to_string(sender->id) + ") by '" + command + "' ***\r\n";
		
		strcpy(receiver->pipefrom_msg, msg.c_str());
		receiver->isPipeFrom = true;

		notify(receiver->ppid);
		while (receiver->isPipeFrom){
			;
		}
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
		write(fd, "% ", 2);
	}
	else if (!isUserPipeExist(sender->id, receiver->id)){				/*  如果 userPipeList 中 沒有 from->to 的 pipe 的話 (還沒有 pipe 給別人) */
		sender->to_null = false;
		sender->userpipe_to = receiver->id;
		createFIFO(sender->id, receiver->id);
		notify(receiver->pid, true);

		string tmp_sendname = sender->name;
		string tmp_recvname = receiver->name;
		msg = "*** " + tmp_sendname + " (#" + to_string(sender->id) + ") just piped '" + command + "' to " + tmp_recvname + " (#" + to_string(receiver->id) + ") ***\r\n";
		
		strcpy(sender->pipeto_msg, msg.c_str());
		sender->isPipeTo = true;

		notify(sender->ppid);
		while (receiver->isPipeTo){
			;
		}
	}
	else{
		sender->to_null = true;
		msg = "*** Error: the pipe #" + to_string(sender->id) + "->#" + to_string(receiver->id) + " already exists. ***\r\n";
		write(fd, msg.c_str(), msg.length());
		write(fd, "% ", 2);
	}
}



void Users::apply_user_pipeFrom(int fd, int from_id){
	User* sender = getUserById(from_id);
	User* receiver = getUserByFd(fd);
	string fifo_name = baseFifoName + to_string(from_id) + "_" + to_string(receiver->id);
	int readfd;

	// cout << "[>] Read from fifo: " << fifo_name << endl;
	if (receiver->from_null == false){			/* from_null == false 時，代表這個 pipe 存在 */
		for (int i = 0; i < 100; i++){
			string tmp_fname = receiver->fifoList[i].fifoname;
			if (tmp_fname == fifo_name){
				readfd = receiver->fifoList[i].readfd;

				strcpy(receiver->fifoList[i].fifoname, "");
				receiver->fifoList[i].readfd = -1;
				break;
			}
		}

		dup2(readfd, STDIN_FILENO);
		close(readfd);
		unlink(fifo_name.c_str());
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
	string fifo_name = baseFifoName + to_string(sender->id) + "_" + to_string(to_id);
	int writefd;

	// cout << "[>] Write to fifo: " << fifo_name << endl;
	if (sender->to_null == false){				/* to_null == false 時，代表這個 pipe 存在 */
		if ((writefd = open(fifo_name.c_str(), O_WRONLY)) < 0){
			// cerr << "Can't open FIFO write: " << strerror(errno) << endl;
		}

		dup2(writefd, STDOUT_FILENO);
		close(writefd);
	}
	else{										/* to == -1 時，代表 to_user 不存在 */
		int null_fd = open("/dev/null", O_WRONLY);
		dup2(null_fd, STDOUT_FILENO);
		close(null_fd);
		// cerr << "[*] stdout -> '/dev/null/'" << null_fd << endl;
	}
}

bool Users::isUserPipeExist(int from_id, int to_id){
	User* receiver = getUserById(to_id);
	string fifoName = baseFifoName + to_string(from_id) + "_" + to_string(to_id);

	for (int i = 0; i < 100; i++){
		string tmp_fifo_name = receiver->fifoList[i].fifoname;
		if (tmp_fifo_name == fifoName)
			return true;
	}
	return false;
}


void Users::showUser(int fd){
	User* u = getUserByFd(fd);
	cout << "User id: [" << u->id << "]: " << u->name << ", ip: " << u->ip << endl;
}


/* 把存在 userList[MAXCLIENT].trashcan 的 fifo 檔案全部 unlink */
void Users::clearUserPipe(){
	for (int i = 0; i < 100; i++){
		if (strlen(userList[MAXCLIENT].allfifo[i]) != 0){
			unlink(userList[MAXCLIENT].allfifo[i]);
			// cout << "[*] Unlinking FIFO: " << userList[MAXCLIENT].allfifo[i] << endl;
		}
	}
}


/* 收到 signal 之後，就去 share memory 中拿 fifo name 上來，放到 fifoList 中 */
void Users::OpenFifoBySignal(int recv_pid){
	User* receiver = getUserByPid(recv_pid);
	int readfd;

	for (int i = 0; i < 100; i++){
		if (strlen(receiver->fifoList[i].fifoname) == 0){		/* 代表為空的 */
			strcpy(receiver->fifoList[i].fifoname, receiver->writeFifoHere);		/* 把 fifo name 從底下 buffer 抓上來 */
			strcpy(receiver->writeFifoHere, "");									/* 把 buffer 清空 */		
			
			if ((readfd = open(receiver->fifoList[i].fifoname, O_RDONLY)) < 0){
				// cerr << "Can't open FIFO read: " << strerror(errno) << endl;
			}
			else{
				receiver->fifoList[i].readfd = readfd;
				// cout << "Open FIFO: " << receiver->fifoList[i].fifoname << ", fd: " << receiver->fifoList[i].readfd << endl;
			}
			break;
		}
	}
}