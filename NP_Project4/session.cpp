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
#include "connect_session.cpp"
#include "bind_session.cpp"
#include "header.h"

using boost::asio::ip::tcp;
using namespace std;


class session : public enable_shared_from_this<session> {
public:
	session(boost::asio::io_context& io_context, tcp::socket socket)
	: client_(move(socket)), io_context(io_context) {
	}

	void start() {
		auto self(shared_from_this());
		client_.async_read_some(boost::asio::buffer(socks_request, max_length),
			[this, self](boost::system::error_code ec, size_t length) {
				if (!ec) {
					requestPkt.VN = to_string(socks_request[0]);
					requestPkt.CD = to_string(socks_request[1]);
					requestPkt.DSTPORT = to_string((int(socks_request[2]) << 8) + int(socks_request[3]));
					requestPkt.DSTIP = to_string(socks_request[4]) + "." + to_string(socks_request[5]) + "." + to_string(socks_request[6]) + "." + to_string(socks_request[7]);
					requestPkt.DOMAIN_NAME = getDomainName(length);

					if (requestPkt.CD == "1") {			/* connect request */
						make_shared<connect_session>(io_context, move(client_), requestPkt)->start();
						// cout << "make connect_session" << endl;
					}
					else if (requestPkt.CD == "2") {
						make_shared<bind_session>(io_context, move(client_), requestPkt)->start();
						// cout << "make bind_session" << endl;
					}
				}
			});
	}
private:
	string getDomainName(size_t length) {
		vector<int> null_pos;
		for (size_t i = 8; i < length; i++) {
			if (socks_request[i] == 0x00) {
				null_pos.push_back(i);
			}
		}

		if (null_pos.size() == 1) {
			return "";
		}
		else {
			int start = null_pos[0] + 1;
			int end = null_pos[1];
			string domainName;

			for (int i = start; i < end; i++) {
				domainName += socks_request[i];
			}
			return domainName;
		}
	}

	tcp::socket client_;
	boost::asio::io_context& io_context;

	enum { max_length = 1024 };
	unsigned char socks_request[max_length] = { 0 };
	RequestPacket requestPkt;
};