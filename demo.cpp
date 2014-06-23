
/*
	Description : Simple multithreaded web server app in C++ (for educational porpuses only)
	Author		: Dimitris Vlachos (DimitrisV22@gmail.com) (http://github.com/DimitrisVlachos)
	Date		: 22/6/2014
	Instructions:
				-Compile with : g++ mt.cpp server_demo.cpp -O2 -lpthread -o server
				-Create a directory that matches work_dir argument passed in web_server_c::init() function and place your files
				-Run : ./server (Due to port 80 allocation you (most-likely) have to run the server as root)
*/

#include "jweb_srv.hpp"

int main() { 
	const std::string work_dir = "root";
	const std::string php_cgi_path = "/usr/bin/php-cgi/";	
	const std::string php_cgi_binary = "php-cgi";
	const int32_t default_port = 80;
	const uint32_t max_threads = 4;
	web_server_c www;

	if (!www.init(work_dir,default_port,max_threads)) {
		std::cout << "Init failed : " << www.get_error_string() << std::endl;
		return -1;
	}

	//optional
	if (!www.init_php(php_cgi_path,php_cgi_binary)) {
		std::cout << "Warning : Init PHP CGI SCRIPT failed : " << www.get_error_string() << std::endl;
	}

	do {	
	} while (www.run());

	www.shutdown();
	return 0;
}
