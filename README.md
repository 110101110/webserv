# WEBSERV

*This project has been created as part of the 42 curriculum by qizhang, elanteno, hazali.*

### Description
**Webserv** is a lightweight HTTP/1.0 server written entirely in C++98 from scratch with no external libraries. Emulating core behaviors of NGINX, the server aims to handle multiple concurrent client connections using non-blocking I/O and socket-multiplexing. It features a state machine that supports complex parsing of NGINX-style configuration files, virtual host management, static content and Common Gateway Interface (CGI) script execution.

**Key features**
* Non-blocking I/O multiplexing utilizing one single `poll()` call.
* A configuration parser supporting cascading inheritance, virtual hosts, and distinct location blocks.
* Support for standard HTTP methods (GET, POST, DELETE) with customizable payload limits.
* Static file serving, directory lisitng (`autoindex`), and custom error pages routing.
* CGI execution with strict timeout management to prevent server blocking.

### Instructions
<!-- compilation, installation and execution -->
**Prerequistes**
* A C++98 compatible compiler (e.g., `clang++` or `g++`)
* A UNIX-like system (Linux or MacOs)

**Installation**
* Clone the repository to your local machine
* Navigate into project repository: `cd webserv`
* Compile the source code using the Makefile in root: `make`. Available rules are `all`, `clean`, `fclean` and `re`.

**Usage**

* To run the server: `./webserv [configuration file]` (e.g., `./webserv conf/default.config`)

* To connect to the server: `telnet`, `curl` or standard web browser (e.g., `telnet http://localhost:8080`)

* To shut down the server: press `Ctrl+C` in the terminal

### Resources
<!-- references and documentation -->
* https://datatracker.ietf.org/doc/html/rfc1945#section-4.1
* https://www.garshol.priv.no/download/text/http-tut.html
* Beej's Guide to Network Programming
* https://nginx.org/en/docs/
