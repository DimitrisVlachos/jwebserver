
/*

	Description : Simple multithreaded web server app in C++ (for educational porpuses only)
	Author		: Dimitris Vlachos (DimitrisV22@gmail.com) (http://github.com/DimitrisVlachos)
	Date		: 22/6/2014
	Instructions:
				-Compile with : g++ mt.cpp server_demo.cpp -O2 -lpthread -o server
				-Create a directory that matches work_dir argument passed in web_server_c::init() function and place your files
				-Run : ./server (Due to port 80 allocation you (most-likely) have to run the server as root)
*/

#ifndef ___jweb_srv__hpp___
#define  ___jweb_srv__hpp___

#include "types.hpp"
#include "mt.hpp"

#define DEBUG_THREADS
#define DEBUG_RCV
#define BLOCK_RCV_READS

namespace web_server_private {
	class byte_buffer_c;

	enum parse_result_e {
		parse_result_ok = 0,
		parse_result_bad_path,
		parse_result_empty_path,
		parse_result_unk_error
	};

	//used for extension to html content type conversion
	struct ext_map_t {
		const char* extension;
		const char* type;
		bool binary_type;
	};

	struct server_config_ctx_t {
		int32_t port;
		int32_t socket;
		struct sockaddr_in addr;
	};

	struct client_session_ctx_t {
		socklen_t sin_len;
		int32_t socket;
		struct sockaddr_in addr;
		std::string php_cgi_binary;
		std::string php_cgi_path;
	};

	struct mt_args_t {
		uint32_t id;
		std::string root;
		web_server_private::client_session_ctx_t* cfg;
	};

	static const ext_map_t g_ext_to_type[] = {
		{"txt","text/plain",false},
		{"htm","text/html",false},
		{"html","text/html",false},
		{"php","text/html",false},
		{"gif","image/gif",true},
		{"jpg","image/jpg",true},
		{"jpeg","image/jpeg",true},
		{"png","image/png",true},
		{"pnga","image/pnga",true},
		{"ico","image/ico",true},
		{"bmp","image/bmp",true},
		{"gz","image/gz",true},
		{"tar","image/tar",true},
		{"zip","image/zip",true},
		{"pdf","application/pdf",true},
		{"zip","application/octet-stream",true},
		{"rar","application/octet-stream",true},
		{NULL,NULL,false} 
	};

	static const bool file_exists(const std::string& s) ;
	static const bool directory_exists(const std::string& s) ;
	static const bool is_safe_path(const std::string& root,const std::string& s);
 
	static const bool directory_exists(const std::string& path)  {
		struct stat s;
		int32_t ret = stat(path.c_str(), &s);
		if(-1 == ret)
			return false;

		return S_ISDIR(s.st_mode);
	}

	//Simple security check : Dont accept requests that look like ../../../ 
	//and also make sure that it is a valid file system entry....
	static const bool is_safe_path(const std::string& root,const std::string& s)  {
		if (s.empty())
			return false;

		if (s[0] == '.')
			return false;
		
		if (file_exists(root + "/" + s))
			return true;

		return directory_exists(root + "/" + s);
	}

	static const bool file_exists(const std::string& path)  {
		struct stat s;
		int32_t ret = stat(path.c_str(), &s);
		if(-1 == ret)
			return false;

		return !S_ISDIR(s.st_mode);
	}

	static const ext_map_t* file_ext_map_to_media_type(const char* fn) {
		int len = strlen(fn);

		for (;--len > 0;) {
			if (fn[len] == '.') {
				fn = &fn[len+1];
				int next = 0;
				for (;;++next) {
					const char* ext = g_ext_to_type[next].extension;
					if (NULL == ext)
						break;
	
					if (0 == strcmp(ext,fn))
						return &g_ext_to_type[next];
				}
				break;
			}
		}

		return &g_ext_to_type[0];
	}
 
	static bool xfer_stream(web_server_private::client_session_ctx_t* cfg,const char* file_content) {
		char buf[4096];
		const ext_map_t* ext = file_ext_map_to_media_type(file_content);
		int32_t fd;
		uint64_t len;
		struct stat64 st;

		fd =  open64(file_content,O_RDONLY);
		if (fd < 0) {
			std::cout << "Cant open " << file_content << std::endl;
			return false;
		}

		if (fstat64(fd,&st) < 0) {
			std::cout << "fstat64 fail " << file_content << std::endl;
			close(fd);
			return 0;
		}

    	len = st.st_size;

 
		{
			sprintf(buf,"HTTP/1.1 200 OK\nContent-length: %d\nContent-Type: %s\n\n",len,ext->type);
			send(cfg->socket,&buf,strlen(buf),0);
		}

		uint32_t i = 0,j = sizeof(buf),k = j;
	
		for (;i < len;i += k) {
			
			if ((i + j) > len) 
				k = len - i;
			else
				k = j;
		
			read(fd,(void*)buf,k);
			send(cfg->socket,&buf,k,0);
		}

		close(fd);
		return true;
	}

	//Just to get rid of warning:"deprecated conversion from string constant to ‘char*’ [-Wwrite-strings]"
	static inline void put_env(const char* s) {
		std::string tmp = std::string(s);
		putenv((char*)&tmp[0]);
	}

	static parse_result_e parse_request(const std::string& root,web_server_private::client_session_ctx_t* cfg) {
		char* ds;
		std::vector<char> buffer;

#ifdef BLOCK_RCV_READS
		{	//Block reads :)
			char fetch[4096];
			bool done = false;
			do {
				int32_t ret = recv(cfg->socket,&fetch,sizeof(fetch),0);
				if (-1 == ret)
					break;
				for (uint32_t i = 0,j=(uint32_t)ret;i < j;++i) {	//Can be improved
					buffer.push_back(fetch[i]);
					if (buffer.size() >= 2U) {
						uint32_t offs = buffer.size() - 1;
						if ((buffer[offs] == '\n') && (buffer[offs - 1] == '\r')) {
							done = true;
							break;
						}
					}
				}
				//if (ret < sizeof(fetch))
					//break;
			} while (!done);
		}
#else
		do {	//Byte reads
			char fetch;
			int32_t ret =  recv(cfg->socket,&fetch,1,0);
 
			if (-1 == ret) {
				break;
			}

			buffer.push_back(fetch);
			if (buffer.size() >= 2U) {
				uint32_t offs = buffer.size() - 1;
				if ((buffer[offs] == '\n') && (buffer[offs - 1] == '\r')) {
					break;
				}
			}
		} while (1);
#endif

		if (buffer.empty())
			return parse_result_ok;

#ifdef DEBUG_RCV
		//buffer address might be reallocated before assignment in ds so expand here ;)
		buffer.push_back('\0');
#endif
		 
		ds = (char*)&buffer[0];
#ifdef DEBUG_RCV
		printf("Got %s\n",ds);
#endif
		//Handle more requests if you like....
		char* s = strstr(ds,"GET /");
 

		if (s) {
			s += strlen("GET /");
			if (s[1] != ' ') {
				char* p = &s[0];
				while (*p && *p != ' ')++p;
				*p = '\0';
				if ((strlen(s) != 0) && (*s != ' ')) {
		
					const ext_map_t* mtype = file_ext_map_to_media_type(s);
					bool unsafe = false;
					if (strcmp(mtype->extension,"php") == 0) {
						if (cfg->php_cgi_binary.empty() || cfg->php_cgi_path.empty())
							unsafe = true;
						else {
							if (!is_safe_path(root,std::string(s))) {
								unsafe = true;
							} else {
								std::string path = root + "/" + std::string(s);
								std::string arg = "SCRIPT_FILENAME=" + path;
								if ((!file_exists(path)) || (!file_exists(cfg->php_cgi_path + "/" + cfg->php_cgi_binary)))
									unsafe = true;
								else {
									{
										char buf[128];
										sprintf(buf,"HTTP/1.1 200 OK\nConnection: close\n");
										send(cfg->socket,&buf,strlen(buf),0);
									}
									
									put_env("GATEWAY_INTERFACE=CGI/1.1");
									put_env(arg.c_str());
									put_env("QUERY_STRING=");
									put_env("REDIRECT_STATUS=true");
									put_env("REQUEST_METHOD=GET");
									put_env("SERVER_PROTOCOL=HTTP/1.1");
									put_env("REMOTE_HOST=127.0.0.1");
									dup2(cfg->socket,STDOUT_FILENO);
									execl(cfg->php_cgi_path.c_str(),cfg->php_cgi_binary.c_str(),0);
								}
							}
						}
					}

					if ((unsafe) || (!is_safe_path(root,std::string(s)))) { 
						char buf[128];
						sprintf(buf,"HTTP/1.1 200 OK\nContent-length: %d\nContent-Type: %s\n\n404 doesnt exist:)",18,"text/plain");
						send(cfg->socket,&buf,strlen(buf),0);
						return parse_result_bad_path;
					}


					if (!xfer_stream(cfg,std::string(root + "/" + s).c_str()))
						return parse_result_unk_error;

					return parse_result_ok;
				}
			}

			//Just GET / indicates index access
			static const char* entry_point[] = {
				"/index.htm",
				"/index.html",
				"/main.html",
				0
			};

			if ((!cfg->php_cgi_binary.empty()) && (!cfg->php_cgi_path.empty()) && file_exists(root + "/index.php")
				&& file_exists(cfg->php_cgi_path + "/" + cfg->php_cgi_binary)) { 
				{
					char buf[128];
					sprintf(buf,"HTTP/1.1 200 OK\nConnection: close\n");
					send(cfg->socket,&buf,strlen(buf),0);
				}
	
				std::string arg = "SCRIPT_FILENAME=" + root + "/index.php";
				put_env("GATEWAY_INTERFACE=CGI/1.1");
				put_env(arg.c_str());
				put_env("QUERY_STRING=");
				put_env("REDIRECT_STATUS=true");
				put_env("REQUEST_METHOD=GET");
				put_env("SERVER_PROTOCOL=HTTP/1.1");
				put_env("REMOTE_HOST=127.0.0.1");
				dup2(cfg->socket,STDOUT_FILENO);
				execl(cfg->php_cgi_path.c_str(),cfg->php_cgi_binary.c_str(),0);

				return parse_result_ok;
			} 

			for (uint32_t i = 0;entry_point[i] != 0;++i) {
				const std::string fn = root+std::string(entry_point[i]);
				if (file_exists(fn)) { 
					if (!xfer_stream(cfg,std::string(fn).c_str()))
						return parse_result_unk_error;

					return parse_result_ok;
				} 
			}
		}


		return parse_result_empty_path;
	}

	static mt_entry_point_result_t thread_entry_point( mt_entry_point_stack_args ) {
		mt_args_t* args;
	
		mt_entry_point_prologue(args,mt_args_t); 
#ifdef DEBUG_THREADS
		printf("Enter thread %u\n",args->id);
#endif
		web_server_private::parse_request(args->root,args->cfg);
		close(args->cfg->socket);
		delete args->cfg;
		mt_wr_stat(args->id,mt_stat_idle);
#ifdef DEBUG_THREADS
		printf("Leave thread %u\n",args->id);
#endif
		mt_entry_point_epilogue(NULL);
	}
};

class web_server_c {
	private:
	bool m_initialized,m_silent;
	uint32_t m_nb_threads;

	std::vector<web_server_private::mt_args_t> m_thread_args;
	std::string m_root;
	std::string m_error_string;
	std::string m_php_cgi_binary;
	std::string m_php_cgi_path;
	web_server_private::server_config_ctx_t m_server_cfg;

	template <typename base_t>
	inline base_t s_to_n(std::string s) {
		std::istringstream strm(s);
		base_t r;
		strm >> r;
		return r;
	}

	template <typename base_t>
	inline std::string n_to_s(base_t n) {
		std::ostringstream strm;
     	strm << n;
		return strm.str();
	}


	public:

	web_server_c() : m_initialized(false),m_silent(false) {}
	~web_server_c() { shutdown(); }

	inline void set_silent(const bool silent) {
		m_silent = silent;
	}

	inline const std::string& get_error_string() const {
		return m_error_string;
	}

	bool run() {
		if (!m_initialized)
			return false;

		web_server_private::client_session_ctx_t* cfg = new web_server_private::client_session_ctx_t();
		cfg->sin_len = sizeof(struct sockaddr_in);
		cfg->socket = accept(m_server_cfg.socket,(struct sockaddr*)&cfg->addr,&cfg->sin_len);
		cfg->php_cgi_binary = m_php_cgi_binary;
		cfg->php_cgi_path = m_php_cgi_path;

		if (!m_silent)
			std::cout << "New connection:" << inet_ntoa(cfg->addr.sin_addr)  << std::endl;

		if (m_nb_threads < 2) {
			web_server_private::parse_request(m_root,cfg);
			close(cfg->socket);
			delete cfg;
		} else {
			for (uint32_t i = 0;i < m_nb_threads;++i) {
				web_server_private::mt_args_t* args = &m_thread_args[i];
				if (mt_stat_idle == mt_rd_stat(args->id)) {
					mt_wr_stat(args->id,mt_stat_busy);
					args->cfg = cfg;
					mt_call_thread(web_server_private::thread_entry_point,args,i);
					return m_initialized;
				}
			}

			web_server_private::parse_request(m_root,cfg);
			close(cfg->socket);
			delete cfg;
		}

		return m_initialized;
	}

	bool init_php(const std::string php_cgi_binary,const std::string& php_cgi_path) {
		if (!m_initialized) {
			m_error_string = "init_php : Call init() first\n";
			return false;
		}

		m_php_cgi_binary = php_cgi_binary;
		m_php_cgi_path = php_cgi_path;

		if (!m_php_cgi_path.empty()) {
			if (m_php_cgi_path[m_php_cgi_path.length()-1] == '/')
				m_php_cgi_path = m_php_cgi_path.substr(0,m_php_cgi_path.length()-1);
		}

		if (!web_server_private::file_exists(m_php_cgi_path + "/" + m_php_cgi_binary )) {
			m_error_string = "Php binary not found!\n";
			m_php_cgi_binary = m_php_cgi_path = "";
			return false;
		}



		return true;
	}

	bool init(const std::string& work_directory,const int32_t port,const uint32_t nb_threads = 1,const bool silent = false) {
		if (m_initialized)
			this->shutdown();

		if (silent)	//Force update only if needed
			m_silent = true;

		mt_shutdown();
		
		m_thread_args.clear();

		if (nb_threads > 1) {
			for (uint32_t i = 0;i < nb_threads;++i) {
				m_thread_args.push_back(web_server_private::mt_args_t());
				m_thread_args.back().id = i;
				m_thread_args.back().root = work_directory;
			}

			mt_init(nb_threads);			
		}

		m_nb_threads = nb_threads;
		m_root = work_directory;
		m_server_cfg.port = port;
		m_server_cfg.socket = socket(AF_INET,SOCK_STREAM,0);

		if (!m_root.empty()) {
			if (m_root[m_root.length()-1] == '/')
				m_root = m_root.substr(0,m_root.length()-1);
		}

		if (m_server_cfg.socket == -1) {
			m_error_string = "m_server_cfg.socket == -1";
			return false;
		}

		int32_t opt_data = 1;
		if (setsockopt(m_server_cfg.socket,SOL_SOCKET,SO_REUSEADDR/*SO_KEEPALIVE*/,&opt_data,sizeof(int32_t)) == -1) {
			m_error_string = "setsockopt == -1";
			return false;
		}
		     
		m_server_cfg.addr.sin_family = AF_INET;         
		m_server_cfg.addr.sin_port = htons(m_server_cfg.port);     
		m_server_cfg.addr.sin_addr.s_addr = INADDR_ANY; 
		memset((void*)&m_server_cfg.addr.sin_zero,0,8);

		if (bind(m_server_cfg.socket,(struct sockaddr*)&m_server_cfg.addr,sizeof(struct sockaddr))== -1) {
			m_error_string = "bind == -1 (No proper perms?)";
			return false;
		}
	 
		if (listen(m_server_cfg.socket,SOMAXCONN) == -1)  {
			m_error_string = "listen == -1";
			return false;
		}

		if (!m_silent) {
			std::cout << "Listenning @" << inet_ntoa(m_server_cfg.addr.sin_addr) << ":" << port << std::endl;
			std::cout << "Number of threads : " << nb_threads << std::endl;
		}

		m_initialized = true;
		return true;
	}

	void shutdown() {
		if (!m_initialized) {
			m_error_string = "shutdown() called without previous successful init()";
			return;
		}

		if (m_nb_threads > 1) {
			for (uint32_t i = 0;i < m_nb_threads;++i) {
				web_server_private::mt_args_t* args = &m_thread_args[i];
				while (mt_stat_idle != mt_rd_stat(args->id)) {
				}
			}		
		}

		mt_shutdown();
		close(m_server_cfg.socket);
		m_initialized = false;
		m_error_string = "";
	}	
};

#endif
