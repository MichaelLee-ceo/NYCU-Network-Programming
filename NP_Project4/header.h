#ifndef MYSTRUCT_H
#define MYSTRUCT_H

#include <string>
using namespace std;

struct RequestPacket {
	string VN;
	string CD;
	string DSTIP;
	string DSTPORT;
	string DOMAIN_NAME;
};

#endif