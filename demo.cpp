
/*
	Description : Simple multithreaded web server app in C++ (for educational porpuses only)
	Author		: Dimitris Vlachos (DimitrisV22@gmail.com) (http://github.com/DimitrisVlachos)
	Date		: 22/6/2014
	Instructions:
				-Compile with : g++ mt.cpp server_demo.cpp -O2 -lpthread -o server
				-Run : ./server (Due to port 80 allocation you (most-likely) have to run the server as root)
*/

#include "jweb_srv.hpp"

int main() { 
	const std::string work_dir = "root";
	const int32_t default_port = 8080;
	const uint32_t max_threads = 4;
	web_server_c www;

	if (!www.init(work_dir,default_port,max_threads)) {
		std::cout << "Init failed : " << www.get_error_string() << std::endl;
		return -1;
	} 

	do {	
	} while (www.run());

	www.shutdown();
	return 0;
}
