#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <memory>
#include <utility>
#include <boost/format.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/classification.hpp>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

struct Connection {
	string host;
	string port;
	string file;
};

string header = "Content-type: text/html\r\n\r\n";
string get_panel_page() {
	string panel_part1 = R"(
	<!DOCTYPE html>
	<html lang="en">
	  <head>
	    <title>NP Project 3 Panel</title>
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
	      href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
	    />
	    <style>
	      * {
	        font-family: 'Source Code Pro', monospace;
	      }
	    </style>
	  </head>
	  <body class="bg-secondary pt-5">
	  	<form action="console.cgi" method="GET">
	      <table class="table mx-auto bg-light" style="width: inherit">
	        <thead class="thead-dark">
	          <tr>
	            <th scope="col">#</th>
	            <th scope="col">Host</th>
	            <th scope="col">Port</th>
	            <th scope="col">Input File</th>
	          </tr>
	        </thead>
	        <tbody>
	)";

	string host_menu;
	for (int i = 0; i < 12; i++) {
		host_menu += "<option value=\"nplinux" + to_string(i+1) + ".cs.nctu.edu.tw\">nplinux" + to_string(i+1) + "</option>";
	}

	string panel_part2;
	int N_SERVERS = 5;
	for (int i = 0; i < N_SERVERS; i++) {
		panel_part2 += (boost::format(R"(
			<tr>
	          <th scope="row" class="align-middle">Session %1%</th>
	          <td>
	            <div class="input-group">
	              <select name="h%2%" class="custom-select">
	                <option></option>"%3%"
	              </select>
	              <div class="input-group-append">
	                <span class="input-group-text">.cs.nctu.edu.tw</span>
	              </div>
	            </div>
	          </td>
	          <td>
	            <input name="p%2%" type="text" class="form-control" size="5" />
	          </td>
	          <td>
	            <select name="f%2%" class="custom-select">
	              <option></option>
	              <option value="t1.txt">t1.txt</option>
	              <option value="t2.txt">t2.txt</option>
	              <option value="t3.txt">t3.txt</option>
	              <option value="t4.txt">t4.txt</option>
	              <option value="t5.txt">t5.txt</option>
	            </select>
	          </td>
	        </tr>
			)") % to_string(i+1) % to_string(i) % host_menu).str();
	}
	string panel_part3 = R"(
			<tr>
	          <td colspan="3"></td>
	          <td>
	            <button type="submit" class="btn btn-info btn-block">Run</button>
	          </td>
	        </tr>
	      </tbody>
	    </table>
	  </form>
	  </body>
	  </html>
	)";

	return header + panel_part1 + panel_part2 + panel_part3;
}

string get_console_page(vector<Connection> cnt) {
	string console_head = R"(
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
	string console_part1;
	for (int i = 0; i < cnt.size(); i++) {
		if (cnt[i].host != "" && cnt[i].port != "" && cnt[i].file != "") {
			console_part1 += "<th scope=\"col\">" + cnt[i].host + ":" + cnt[i].port + "</th>\r\n";
		}
	}

	string console_part2 = R"(
				</tr>
		      </thead>
		      <tbody>
		        <tr>
	)";

	string console_part3;
	for (int i = 0; i < cnt.size(); i++) {
		if (cnt[i].host != "" && cnt[i].port != "" && cnt[i].file != "") {
			console_part3 += "<td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>\r\n";
		}
	}
	string console_part4 = R"(
		        </tr>
		      </tbody>
		    </table>
		  </body>
		</html>
	)";

	return header + console_head + console_part1 + console_part2 + console_part3 + console_part4;
}  	



class remote_session : public enable_shared_from_this<remote_session> {
public:
	remote_session(boost::asio::io_context& io_context, tcp::socket socket, shared_ptr<tcp::socket> client_socket, Connection connection, int sid)
	: socket_(move(socket)), client_socket_(client_socket), resolver_(io_context), query_(connection.host, connection.port) {
		file = connection.file;
		session_id = sid;

		read_file();
	}

	void start() {
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
	void do_connect() {
		auto self(shared_from_this());
		socket_.async_connect(endpoint_,
			[this, self](boost::system::error_code ec) {
				if (!ec) {
					cout << "[+] Connect to server: " << socket_.remote_endpoint().address() << ":" << socket_.remote_endpoint().port() << ", sid: " << session_id << endl;
					do_read();
				}
			});
	}

	void do_read() {
		auto self(shared_from_this());
		socket_.async_read_some(boost::asio::buffer(output_, max_length),
			[this, self](boost::system::error_code ec, size_t length) {
				if (!ec) {
					string output = output_;
					string output_html = output_shell(output);
					do_write_client(output_html);

					memset(output_, '\0', sizeof(output_));
					memset(command_, '\0', sizeof(command_));

					if (output.find("%") != string::npos) {
						string curr_cmd = get_command();
						string output_command_html = output_command(curr_cmd);
						do_write_client(output_command_html);

						// cout << "[" << session_id << "] " << curr_cmd;

						strcpy(command_, curr_cmd.c_str());
						do_write_remote();
					}
					else {
						do_read();
					}
				}
			});
	}

	void do_write_client(string result) {
		auto self(shared_from_this());
		boost::asio::async_write(*client_socket_, boost::asio::buffer(result.c_str(), result.length()),
			[this, self, result](boost::system::error_code ec, size_t) {
				if (!ec) {
					// cout << "do_write[" << session_id << "] " << result;
					;
				}
			});
	}

	void do_write_remote() {
		auto self(shared_from_this());
		boost::asio::async_write(socket_, boost::asio::buffer(command_, strlen(command_)),
			[this, self](boost::system::error_code ec, size_t) {
				if (!ec) {
					// cout << "do_write[" << session_id << "] " << command_;
					string curr_cmd = command_;
					if (curr_cmd.find("exit") != string::npos) {
						socket_.close();
						cout << "[-] Session closed: " << session_id << endl;
					}
					else {
						do_read();
					}
				}
			});
	}

	string output_shell(string content) {
		string escaped_content = html_escape(content);
		return + "<script>document.getElementById(\'s" + to_string(session_id) + "\').innerHTML += \'" + escaped_content + "\';</script>";
	}

	string output_command(string content) {
		string escaped_content = html_escape(content);
		return + "<script>document.getElementById(\'s" + to_string(session_id) + "\').innerHTML += \'<b>" + escaped_content + "</b>\';</script>";
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
		if (commands.size() > 0) {
			string cmd = *commands.begin();
			commands.erase(commands.begin());
			return cmd;
		}
		else {
			return "";
		}
	}

	void read_file() {
		ifstream infile("test_case/" + file);
		string line;
		while (getline(infile, line)) {
			commands.push_back(line + "\n");
			// cout << "[>] " << line << endl;
		}
		infile.close();
	}

	shared_ptr<tcp::socket> client_socket_;
	tcp::socket socket_;
	tcp::endpoint endpoint_;
	tcp::resolver resolver_;
	tcp::resolver::query query_;
	string file;
	int session_id;
	vector<string> commands;

	enum { max_length = 1024};
	char command_[max_length] = { 0 };
	char output_[max_length] = { 0 };
};



class client_session : public enable_shared_from_this<client_session> {
public:
	client_session(tcp::socket socket)
		: client_(move(socket)) {
			;
		}

	void start() {					/* 連線成功之後，要讀 (http) request 然後解析 request 的內容 */
		auto self(shared_from_this());
		client_.async_read_some(boost::asio::buffer(data_, max_length),
			[this, self](boost::system::error_code ec, size_t length) {
				if (!ec) {
					string http_request = data_;
					vector<string> request_header;
					vector<string> query;

					boost::split(request_header, http_request, boost::is_any_of("\r\n"), boost::token_compress_on);
					boost::split(query, request_header[0], boost::is_any_of(" "), boost::token_compress_on);

					envp["REQUEST_METHOD"] = query[0];
					envp["REQUEST_URI"] = query[1];
					envp["SERVER_PROTOCOL"] = query[2];
					envp["HTTP_HOST"] = request_header[1].substr(request_header[1].find(" ") + 1);
					envp["SERVER_ADDR"] = client_.local_endpoint().address().to_string();
					envp["SERVER_PORT"] = to_string(client_.local_endpoint().port());
					envp["REMOTE_ADDR"] = client_.remote_endpoint().address().to_string();
					envp["REMOTE_PORT"] = to_string(client_.remote_endpoint().port());

					if (envp["REQUEST_URI"].find("?") != string::npos) {		/* 如果有 quey string 的話 */
						string query_string = envp["REQUEST_URI"];
						envp["QUERY_STRING"] = query_string.substr(query_string.find("?") + 1);
					}
					else {
						envp["QUERY_STRING"] = "";
					}
					
					do_cgi();		/* 解析完 header 之後，就要 send response 然後呼叫對應的 cgi program */
				}
			});
	}

private:
	void do_cgi() {							/* 把 http response 寫回去之後，就要叫對應的 cgi program */
		auto self(shared_from_this());
		strcpy(data_, "HTTP/1.1 200 OK\r\n");
		boost::asio::async_write(client_, boost::asio::buffer(data_, strlen(data_)),
			[this, self](boost::system::error_code ec, size_t) {
				if (!ec) {
					string cgi_program = envp["REQUEST_URI"].substr(0, envp["REQUEST_URI"].find("?"));
					if (cgi_program == "/panel.cgi") {
						write_panel();
					}
					else if (cgi_program == "/console.cgi") {
						vector<string> tmp_query;
						boost::split(tmp_query, envp["QUERY_STRING"], boost::is_any_of("&"), boost::token_compress_on);

						for (int i = 0; i < tmp_query.size(); i += 3) {
							Connection newConnection;
							newConnection.host = tmp_query[i].substr(tmp_query[i].find("=") + 1);
							newConnection.port = tmp_query[i+1].substr(tmp_query[i+1].find("=") + 1);
							newConnection.file = tmp_query[i+2].substr(tmp_query[i+2].find("=") + 1);
							connections.push_back(newConnection);
						}
						write_console();

						shared_ptr<tcp::socket> shared_client_ = make_shared<tcp::socket>(move(client_));

						for (int i = 0; i < connections.size(); i++) {
							if (connections[i].host != "" && connections[i].port != "" && connections[i].file != "") {
								tcp::socket remote_(io_context);
								make_shared<remote_session>(io_context, move(remote_), shared_client_, connections[i], i)->start();
							}
						}
					}
					else {
						client_.close();
					}
				}
			});
	}

	void write_panel() {
		auto self(shared_from_this());
		string panel_page = get_panel_page();		/* 把 http response header + 網頁內容寫回去 */
		boost::asio::async_write(client_, boost::asio::buffer(panel_page.c_str(), panel_page.length()),
			[this, self](boost::system::error_code ec, size_t) {
				if (!ec) {
					cout << "[<--] Finished writen panel page" << endl;
				}
			});
	}

	void write_console() {
		auto self(shared_from_this());
		string console_page = get_console_page(connections);
		boost::asio::async_write(client_, boost::asio::buffer(console_page.c_str(), console_page.length()),
			[this, self](boost::system::error_code ec, size_t) {
				if (!ec) {
					cout << "[<--] Finished writen console page" << endl;
				}
			});
	}


	tcp::socket client_;
	enum { max_length = 1024};
	char data_[max_length] = { 0 };
	char content_[max_length] = { 0 };
	map<string, string> envp;
	vector<Connection> connections;
};


class server {
public:
	server(boost::asio::io_context& io_context, short port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
			do_accept();
		}

private:
	void do_accept() {
		acceptor_.async_accept(			/* 收到 client 發過來的連線 */
			[this](boost::system::error_code ec, tcp::socket socket) {
				if (!ec) {
					cout << "[+] Server receives connection" << endl;
					make_shared<client_session>(move(socket))->start();
				}

				do_accept();
			});
	}

	tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
	try {
		if (argc != 2) {
			cerr << "Usage: async_tcp_server <port>" << endl;
			return 1;
		}

		server s(io_context, atoi(argv[1]));

		cout << "[INFO] Server running..." << endl;

		io_context.run();
	}
	catch (exception& e) {
		cerr << "Exception: " << e.what() << endl;
	}

	return 0;
}