// #include <vector>
// #include <exception>
// #include <unistd.h>
// #include <iostream>
// #include <csignal>
// #include "core/ServerManager.hpp"
// #include "config/ConfigParser.hpp"

// bool g_running = true;

// // void handle_sigint(int sig) {
// // 	(void)sig;
// // 	g_running = false;
// // }


// int main(int argc, char **argv)
// {
// 	if (argc != 2){
// 		std::cerr << "Usage: ./webserv <config_file>" << std::endl;
// 		return 1;
// 	}
// 	try{
// 		ConfigParser parser;
// 		parser.parse(argv[1]);
// 		ServerManager manager(parser.getConfigs());
// 		manager.setupServers();
// 		// manager.run();
// 	}
// 	catch (const std::exception &e){
// 		std::cerr << "Fatal Error: " << e.what() << std::endl;
// 		return 1;
// 	}

// 	return 0;
// }


#include "../includes/core/HttpRequest.hpp"
#include "../includes/core/HttpResponse.hpp"
#include "../includes/core/RequestHandler.hpp"
#include "../includes/config/ServerConfig.hpp"
#include <iostream>

int main()
{
    // 1. créer une config manuelle
    ServerConfig config;
    config.port = 8080;
    config.host = "localhost";
    config.client_max_body_size = 1000000;


	Location loc;
    loc.path = "/";
    loc.root = "www";  // ton dossier www
    loc.index = "index.html";
    loc.autoindex = false;
    loc.methods.push_back("GET");
    loc.methods.push_back("POST");
    loc.methods.push_back("DELETE");
    config.locations.push_back(loc);

	// Location cgi_loc;
	// cgi_loc.path = "/cgi-bin";
	// cgi_loc.root = "/home/hazali/Cursus/webserv/webserv/www";
	// cgi_loc.cgi_ext = ".py";
	// cgi_loc.cgi_path = "/usr/bin/python3"; // résultat de which python3
	// cgi_loc.methods.push_back("GET");
	// cgi_loc.methods.push_back("POST");
	// config.locations.push_back(cgi_loc);

    // 2. simuler une requête
    // std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"; //test1
	// std::string raw = "GET /inexistant.html HTTP/1.1\r\nHost: localhost\r\n\r\n"; // test2
	// std::string raw = "DELETE /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"; //test3 = il faut retirer DELETE des methodes autorise
	// std::string raw = "TOTO /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
	// std::string raw = "GET /cgi-bin/test.py?name=hamdy&age=25 HTTP/1.1\r\nHost: localhost\r\n\r\n"; //debug cgi
	// std::string raw = "POST /cgi-bin/test.py HTTP/1.1\r\n"
    //               "Host: localhost\r\n"
    //               "Content-Type: application/x-www-form-urlencoded\r\n"
    //               "Content-Length: 27\r\n"
    //               "\r\n"
    //               "username=hamdy&password=1234";
	std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost";

    // 3. parser
    HttpRequest req;
    int result = req.parse(raw);
    std::cout << "parse result: " << result << std::endl;



    // 4. générer la réponse
    RequestHandler handler;
    HttpResponse res = handler.handleRequest(req, config);

    // 5. afficher
    std::cout << res.toString() << std::endl;

    return 0;
}
