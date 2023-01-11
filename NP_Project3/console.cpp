#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/classification.hpp>

using boost::asio::ip::tcp;
using namespace std;


string header = "Content-type: text/html\r\n\r\n";
string content_head = R"(
	<!DOCTYPE html>
	<html lang="en">
	  <head>
	    <meta charset="UTF-8" />
	    <title>NP Project 3 Sample Console</title>
	    <link
	      rel="stylesheet"
	      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
	      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
	      crossorigin="anonymous"
	    />
	    <link
	      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
	      rel="stylesheet"
	    />
	    <link
	      rel="icon"
	      type="image/png"
	      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
	    />
	    <style>
	      * {
	        font-family: 'Source Code Pro', monospace;
	        font-size: 1rem !important;
	      }
	      body {
	        background-color: #212529;
	      }
	      pre {
	        color: #cccccc;
	      }
	      b {
	        color: #01b468;
	      }
	    </style>
	  </head>
	  <body>
	    <table class="table table-dark table-bordered">
	      <thead>
	        <tr>
)";
string content_tail1 = R"(
			</tr>
	      </thead>
	      <tbody>
	        <tr>
)";
string content_tail2 = R"(
	        </tr>
	      </tbody>
	    </table>
	  </body>
	</html>
)";


struct Connection {
	string host;
	string port;
	string file;
};


class session : public enable_shared_from_this<session> {
public:
	session(boost::asio::io_context& io_context, tcp::socket socket, Connection connection, int sid)
	: socket_(move(socket)), resolver_(io_context), query_(connection.host, connection.port) {
		file = connection.file;
		session_id = sid;

		read_file();
	}

	void start() {						/* 先解析 nplinux10.cs.nctu.edu.tw, 7879 -> Ip address, port，建立 endpoint_ */
		auto self(shared_from_this());
		resolver_.async_resolve(query_,
			[this, self](boost::system::error_code ec, tcp::resolver::iterator it) {
				if (!ec) {
					endpoint_ = it->endpoint();
					do_connect();
				}
			});
	}

private:
	void do_connect() {					/* 先對 endpoint 建立連線 */
		auto self(shared_from_this());
		socket_.async_connect(endpoint_,
			[this, self](boost::system::error_code ec) {
				if (!ec) {
					cerr << "[+] Connect to: " << endpoint_.address() << ":" << endpoint_.port() << endl;
					do_read();			/* 連線成功之後，就先從 nplinux server 讀 output 回來 */
				}
			});
	}

	void do_read() {					/* 工作內容； 從 nplinux server 讀 output 回來之後，要把下一個 command 寫過去 */
		auto self(shared_from_this());
		socket_.async_read_some(boost::asio::buffer(output_, max_length),
			[this, self](boost::system::error_code ec, size_t length) {
				if (!ec) {
					string output = output_;
					output_shell(output);

					// cerr << output << endl;

					memset(output_, '\0', sizeof(output_));
					memset(command_, '\0', sizeof(command_));

					if (output.find("%") != string::npos) {		/* 讀到 % 時，要把下一個 command 寫過去 npserver */
						string curr_cmd = get_command();
						output_command(curr_cmd);
						strcpy(command_, curr_cmd.c_str());
						do_write();
					}
					else {										/* 還沒收到 % 的話，代表資料還沒傳完，就繼續讀 */
						do_read();
					}
				}
			});
	}

	void do_write() {
		auto self(shared_from_this());
		boost::asio::async_write(socket_, boost::asio::buffer(command_, strlen(command_)),
			[this, self](boost::system::error_code ec, size_t) {
				if (!ec) {
					cerr << "[" << session_id << "->] cmd: " << command_;

					string curr_cmd = command_;
					if (curr_cmd.find("exit") != string::npos) {
						socket_.close();
					}
					else {
						do_read();								/* 寫完 command 之後，就叫 do_read() 讀結果 */
					}							
				}
			});
	}

	void output_shell(string content) {
		string escaped_content = html_escape(content);
		cout << "<script>document.getElementById(\'s" << session_id << "\').innerHTML += \'" << escaped_content << "\';</script>" << flush;
	}

	void output_command(string content) {
		string escaped_content = html_escape(content);
		cout << "<script>document.getElementById(\'s" << session_id << "\').innerHTML += \'<b>" << escaped_content << "</b>\';</script>" << flush;
	}

	string html_escape (string context) {
		boost::replace_all(context, "&", "&amp;");
		boost::replace_all(context, ">", "&gt;");
		boost::replace_all(context, "<", "&lt;");
		boost::replace_all(context, "\"", "&quot;");
		boost::replace_all(context, "\'", "&apos;");
		boost::replace_all(context, "\n", "&NewLine;");
		boost::replace_all(context, "\r", "");
		return context;
	}

	string get_command() {
		if (commands.size() > 0){
			string cmd = *commands.begin();
			commands.erase(commands.begin());
			return cmd;
		}
		else {
			return "";
		}
	}

	void read_file() {
		ifstream infile("./test_case/" + file);
		string line;
		while (getline(infile, line)) {
			commands.push_back(line + "\n");
		}
		infile.close();
	}

	tcp::socket socket_;			/* 連線到 program server 用的 socket */
	tcp::endpoint endpoint_;		/* 要連線到的 program server */
	tcp::resolver resolver_;
	tcp::resolver::query query_;
	string file;
	int session_id;
	vector<string> commands;

	enum { max_length = 1024 };
	char command_[max_length] = { 0 };
	char output_[max_length] = { 0 };
};


vector<Connection> parser(string queryString) {
	vector<Connection> connections;
	vector<string> tmp_query;

	boost::split(tmp_query, queryString, boost::is_any_of("&"), boost::token_compress_on);
	for (int i = 0; i < tmp_query.size(); i += 3) {
		Connection newConnection;
		newConnection.host = tmp_query[i].substr(tmp_query[i].find("=") + 1);
		newConnection.port = tmp_query[i + 1].substr(tmp_query[i + 1].find("=") + 1);
		newConnection.file = tmp_query[i + 2].substr(tmp_query[i + 2].find("=") + 1);
		connections.push_back(newConnection);
	}

	return connections;
}

void setPanel(vector<Connection>& cnt) {
	cout << header << content_head;
	for (int i = 0; i < cnt.size(); i++) {
		if (cnt[i].host != "" && cnt[i].port != "" && cnt[i].file != "") {
			cout << "<th scope=\"col\">" << cnt[i].host << ":" << cnt[i].port << "</th>" << endl;
		}
	}
	cout << content_tail1;
	for (int i = 0; i < cnt.size(); i++) {
		if (cnt[i].host != "" && cnt[i].port != "" && cnt[i].file != "") {
			cout << "<td><pre id=\"s" << i << "\" class=\"mb-0\"></pre></td>" << endl;
		}
	}
	cout << content_tail2;
}


int main (int argc, char* argv[]) {
	try {
		vector<Connection> connections;
		boost::asio::io_context io_context;

		connections = parser(getenv("QUERY_STRING"));		/* 從 quer string 取得每一個要連線到的 host, port, file */
		
		for (int i = 0; i < connections.size(); i++) {		/* 連線到每一個 connection */
			if (connections[i].host != "" && connections[i].port != "" && connections[i].file != "") {
				tcp::socket socket(io_context);				/* 對每一個 remote server，建立一個 session，並分配一個 socket */
				make_shared<session>(io_context, move(socket), connections[i], i)->start();
			}
		}

		setPanel(connections);

		io_context.run();
	}
	catch (exception& e) {
		cerr << "Exception: " << e.what() << endl;
	}

	return 0;
}