#include <iostream>
#include <string>
#include "utils.cpp"
using namespace std;

int main(){
	// regex reg_userpipe(".*?(\\s[<>][0-9]+)");
	// string cmd = "cat <2 > a.txt";
	string command;
	while (getline(cin, command)){
		vector<string> result = userpipe_parser(command);
		for (int i = 0; i < result.size(); i++){
			cout << result[i] << '\t';
		}
		cout << endl;
	}

	return 0;
}