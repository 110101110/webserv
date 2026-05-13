*This project has been created as part of the 42 curriculum by qizhang, elanteno, hazali.*

# Webserv

### Description

**Webserv** is a lightweight HTTP/1.0 server written entirely in C++98 from scratch, with no external libraries. Emulating core behaviors of NGINX, the server handles multiple concurrent client connections using non-blocking I/O and socket multiplexing via a single `poll()` call. It features a configuration parser supporting NGINX-style syntax with cascading inheritance, static file serving, and CGI script execution.

**Key features:**
- Non-blocking I/O multiplexing using a single `poll()` call
- NGINX-style configuration parser with location blocks, and inheritance
- Support for `GET`, `POST`, and `DELETE` HTTP methods with configurable body size limits
- Static file serving, directory listing (`autoindex`), and custom error pages
- CGI execution (Python, shell scripts) with timeout protection against blocking

---

### Instructions

**Prerequisites**
- A C++98-compatible compiler (`g++` or `clang++`)
- A UNIX-like system (Linux or macOS)

**Installation**
```bash
git clone <repository-url>
cd webserv
make
```
Available Makefile rules: `all`, `clean`, `fclean`, `re`.

**Usage**
```bash
# Start the server with a configuration file
./webserv conf/default.conf

# Send a basic request
curl http://localhost:8080/

# Test CGI execution
curl http://localhost:8080/cgi-bin/hello.py

# Test POST with body
curl -X POST -d "key=value" http://localhost:8080/cgi-bin/test.py

# Shut down the server
Ctrl+C
```

You can also connect using `telnet` or any standard web browser.

---

### Resources

**Documentation & references**
- [RFC 1945 — HTTP/1.0 specification](https://datatracker.ietf.org/doc/html/rfc1945)
- [HTTP Tutorial by L.M. Garshol](https://www.garshol.priv.no/download/text/http-tut.html)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [NGINX Documentation](https://nginx.org/en/docs/)
- [The Linux `poll()` man page](https://man7.org/linux/man-pages/man2/poll.2.html)

**Use of AI**

AI assistance (Claude) was used during this project for the following tasks:
- **Debugging**: diagnosing a `std::vector` out-of-bounds crash in the `poll()` event loop caused by iterator invalidation after erasing elements mid-iteration
- **CGI integration**: identifying a missing `cgi_ext`/`cgi_path` configuration causing Python scripts to be served as static files instead of being executed
- **Virtual host routing**: designing and implementing the `_findConfig()` function responsible for selecting the correct `ServerConfig` based on the listening port and the HTTP `Host` header
- **Code review**: reviewing error-handling logic and non-blocking I/O patterns in the event loop
