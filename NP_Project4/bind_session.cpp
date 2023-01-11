#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "header.h"

using boost::asio::ip::tcp;
using namespace std;


class bind_session : public enable_shared_from_this<bind_session> {
public:
	bind_session(boost::asio::io_context& io_context, tcp::socket socket, RequestPacket packet)
	: client_(move(socket)), remote_(io_context), resolver_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), 0)) {
		port = acceptor_.local_endpoint().port();
		requestPkt = packet;
		getAccessControlList();
	}

	void start() {						/* 先解析 SOCKS4 request packet field */
		do_resolve_remote();
	}

private:
	void do_resolve_remote() {
		auto self(shared_from_this());
		string host;
		vector<string> dstIp;
		boost::split(dstIp, requestPkt.DSTIP, boost::is_any_of("."), boost::token_compress_on);
		if ((dstIp[0] == "0") && (dstIp[1] == "0") && (dstIp[2] == "0") && (dstIp[3] != "0")) {
			host = requestPkt.DOMAIN_NAME;
		}
		else {
			host = requestPkt.DSTIP;
		}

		tcp::resolver::query query_(host, requestPkt.DSTPORT);
		resolver_.async_resolve(query_,
			[this, self](boost::system::error_code ec, tcp::resolver::iterator it) {
				if (!ec) {
					endpoint_ = it->endpoint();
					bool isAllowed = canPass();
					if (isAllowed) {
						do_bind_local();
					}
					else {
						send_socks_reject();
					}
				}
			});
	}

	void do_bind_local() {
		auto self(shared_from_this());
		port = acceptor_.local_endpoint().port();
		send_socks_reply(1);
	}

	void do_accept_remote() {
		auto self(shared_from_this());
		acceptor_.async_accept(
			[this, self](boost::system::error_code ec, tcp::socket socket) {
				if (!ec) {
					remote_ = move(socket);
					send_socks_reply(2);
				}
			});
	}

	/* client --> SOCKS --- remote */
	void do_read_client() {
		auto self(shared_from_this());
		client_.async_read_some(boost::asio::buffer(client_buf, max_length),
			[this, self](boost::system::error_code ec, size_t length) {
				if (!ec) {
					do_write_remote(length);
				}
				else {
					client_.close();
				}
			});
	}

	/* client --- SOCKS --> remote */
	void do_write_remote(size_t content_length) {
		auto self(shared_from_this());
		boost::asio::async_write(remote_, boost::asio::buffer(client_buf, content_length),
			[this, self](boost::system::error_code ec, size_t) {
				if (!ec) {
					memset(client_buf, 0x00, sizeof(client_buf));
					do_read_client();
				}
			});
	}

	/* client --- SOCKS <-- remote */
	void do_read_remote() {
		auto self(shared_from_this());
		remote_.async_read_some(boost::asio::buffer(remote_buf, max_length),
			[this, self](boost::system::error_code ec, size_t length) {
				if (!ec) {
					do_write_client(length);
				}
				else {
					remote_.close();
				}
			});
	}

	/* client <-- SOCKS --- remote */
	void do_write_client(size_t content_length) {
		auto self(shared_from_this());
		boost::asio::async_write(client_, boost::asio::buffer(remote_buf, content_length),
			[this, self](boost::system::error_code ec, size_t) {
				if (!ec) {
					memset(remote_buf, 0x00, sizeof(remote_buf));
					do_read_remote();
				}
			});
	}

	void send_socks_reply(int num_reply) {
		auto self(shared_from_this());
		unsigned char p1 = port / 256;
		unsigned char p2 = port % 256;
		unsigned char reply[8] = { 0, 90, p1, p2, 0, 0, 0, 0 };

		boost::asio::async_write(client_, boost::asio::buffer(reply, sizeof(reply)),
			[this, self, num_reply](boost::system::error_code ec, size_t) {
				if (!ec) {
					showMsg("BIND", "Accept");
					if (num_reply == 1) {
						do_accept_remote();
					}
					else if (num_reply == 2) {
						acceptor_.close();
						do_read_client();
						do_read_remote();
					}
				}
			});
	}

	void send_socks_reject() {
		auto self(shared_from_this());
		unsigned char reject[8] = { 0, 91, 0, 0, 0, 0, 0, 0 };
		boost::asio::async_write(client_, boost::asio::buffer(reject, sizeof(reject)),
			[this, self](boost::system::error_code ec, size_t) {
				if (!ec) {
					showMsg("BIND", "Reject");
					acceptor_.close();
					client_.close();
				}
			});
	}

	/* 
		permit c 140.113.*.* 
		permit b *.*.*.*
	*/
	bool canPass() {
		for (size_t i = 0; i < acl.size(); i++) {
			vector<string> record;
			vector<string> aclIp;
			vector<string> dstIp;
			boost::split(record, acl[i], boost::is_any_of(" "), boost::token_compress_on);
			boost::split(aclIp, record[2], boost::is_any_of("."), boost::token_compress_on);
			boost::split(dstIp, endpoint_.address().to_string(), boost::is_any_of("."), boost::token_compress_on);

			if (record[0] == "permit") {
				if (requestPkt.CD == "2" && record[1] == "b") {
					for (int j = 0; j < 4; j++) {
						if (aclIp[j] == "*" || aclIp[j] == dstIp[j]){
							continue;
						}
						else {
							return false;
						}
					}
					return true;
				}
			}
		}
		return false;
	}

	void getAccessControlList() {
		ifstream inFile("./socks.conf");
		string line;
		
		while (getline(inFile, line)) {
			acl.push_back(line);
		}
		inFile.close();
	}

	void showMsg(string command, string reply) {
		cout << "<S_IP>: " << client_.remote_endpoint().address() << endl;
		cout << "<S_PORT>: " << client_.remote_endpoint().port() << endl;
		cout << "<D_IP>: " << endpoint_.address() << endl;
		cout << "<D_PORT>: " << endpoint_.port() << endl;
		cout << "<Command>: " << command << endl;
		cout << "<Reply>: " << reply << endl << endl;
	}

	tcp::socket client_;
	tcp::socket remote_;
	tcp::resolver resolver_;
	tcp::endpoint endpoint_;
	tcp::acceptor acceptor_;

	enum { max_length = 1024 };
	unsigned char client_buf[max_length] = { 0 };
	unsigned char remote_buf[max_length] = { 0 };
	unsigned short port = 0;
	RequestPacket requestPkt;
	vector<string> acl;
};