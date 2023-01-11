#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

using boost::asio::ip::tcp;
using namespace std;

void sig_handler(int sig){
	waitpid(-1, NULL, WNOHANG);
}


class session : public std::enable_shared_from_this<session> {
public:
	session(tcp::socket socket)
		: socket_(move(socket)){
	}

	void start() {
		auto self(shared_from_this());
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			[this, self](boost::system::error_code ec, size_t length) {
				if (!ec) {
					string http_request = data_;
					vector<string> request_header;
					vector<string> query;

					boost::split(request_header, http_request, boost::is_any_of("\r\n"), boost::token_compress_on);		/* 先用 \r\n 去切 HTTP request header */
					boost::split(query, request_header[0], boost::is_any_of(" "), boost::token_compress_on);
					// for (int i = 0; i < request_header.size(); i++){
					// 	cout << i << " " << request_header[i] << endl;
					// }

					envp["REQUEST_METHOD"] = query[0];
					envp["REQUEST_URI"] = query[1];
					envp["SERVER_PROTOCOL"] = query[2];
					envp["HTTP_HOST"] = request_header[1].substr(request_header[1].find(" ") + 1);
					envp["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
					envp["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
					envp["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
					envp["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());

					/* 判斷有沒有 query string */
					if (envp["REQUEST_URI"].find("?") != string::npos) {
						string tmp_uri = envp["REQUEST_URI"];
						envp["QUERY_STRING"] = tmp_uri.substr(tmp_uri.find("?") + 1);
					}
					else {
						envp["QUERY_STRING"] = "";
					}

					do_write();
				}
		});
	}

	void do_write() {
		auto self(shared_from_this());
		strcpy(data_, "HTTP/1.1 200 OK\r\n");
		boost::asio::async_write(socket_, boost::asio::buffer(data_, strlen(data_)),
			[this, self](boost::system::error_code ec, size_t ) {
				if (!ec) {
					pid_t pid = fork();
					if (pid > 0) {				/* parent process */
						socket_.close();
					}
					else if (pid == 0) {		/* child process */
						setenv_();
						dup2(socket_.native_handle(), STDIN_FILENO);
						dup2(socket_.native_handle(), STDOUT_FILENO);
						// dup2(socket_.native_handle(), STDERR_FILENO);

						string tmp_uri = envp["REQUEST_URI"];
						string cgi_program = "." + tmp_uri.substr(0, tmp_uri.find("?"));

						char* argv[] = {(char*) cgi_program.c_str(), NULL};
						if (execvp(argv[0], argv) == -1) {
							cerr << "Execute error: " << strerror(errno) << ", " << argv[0] << endl;
						}
					}
				}
			});
	}

	void setenv_() {
		clearenv();
		for (map<string, string>::iterator it = envp.begin(); it != envp.end(); it++) {
			setenv(it->first.c_str(), it->second.c_str(), 1);
		}
	}

private:
	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length] = { 0 };
	map<string, string> envp;
};


class server {
public:
	server(boost::asio::io_context& io_context, short port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
			do_accept();
	}

private:
	void do_accept() {
		acceptor_.async_accept(
			[this](boost::system::error_code ec, tcp::socket socket) {
				if (!ec) {
					cout << "[+] Server receives connection" << endl;
					make_shared<session>(move(socket))->start();
				}

				do_accept();			/* 要繼續叫 do_accept ()，才能繼續等待新的連線 */
			});
	}

	tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
	signal(SIGCHLD, sig_handler);
	try {
		if (argc != 2) {
			cerr << "Usage: async_tcp_server <port>\n";
			return 1;
		}

		boost::asio::io_context io_context;

		server s(io_context, atoi(argv[1]));

		cout << "[INFO] Server running..." << endl;

		io_context.run();
	}
	catch (exception& e) {
		cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}