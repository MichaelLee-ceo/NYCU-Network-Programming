#include <iostream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include "session.cpp"

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

void sig_handler(int sig){
	waitpid(-1, NULL, WNOHANG);
}


class socksServer {
public:
	socksServer(boost::asio::io_context& io_context, short port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), io_context(io_context) {
			do_accept();
		}
private:
	void do_accept() {
		acceptor_.async_accept(
			[this](boost::system::error_code ec, tcp::socket socket) {
				if (!ec) {
					io_context.notify_fork(boost::asio::io_context::fork_prepare);
					pid_t pid = fork();
					if (pid > 0) {					/* parent process */
						// cout << "\n[+] Server receives connection" << endl;
						io_context.notify_fork(boost::asio::io_context::fork_parent);
						socket.close();
						do_accept();				/* 繼續等待新的連線 */
					}
					else {							/* child process */
						io_context.notify_fork(boost::asio::io_context::fork_child);
						make_shared<session>(io_context, move(socket))->start();
					}
				}
			});
	}
	tcp::acceptor acceptor_;
	boost::asio::io_context& io_context;
};

int main(int argc, char* argv[]) {
	signal(SIGCHLD, sig_handler);
	try {
		if (argc != 2) {
			cerr << "Usage: async_socks_server <port>" << endl;
			return 1;
		}

		socksServer s(io_context, atoi(argv[1]));

		cout << "[INFO] SOCKS Server running..." << endl;

		io_context.run();
	}
	catch (exception& e) {
		cerr << "Exception: " << e.what() << endl;
	}

	return 0;
}