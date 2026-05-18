*This project has been created as part of the 42 curriculum by qizhang, elanteno, hazali.*

# Webserv

### Description

**Webserv** is a lightweight HTTP/1.1 server written entirely in C++98 from scratch, with no external libraries. Emulating core behaviors of NGINX, the server handles multiple concurrent client connections using non-blocking I/O and socket multiplexing via a single `poll()` call. It features a configuration parser supporting NGINX-style syntax with cascading inheritance, static file serving, and CGI script execution.

**Key features:**
- Non-blocking I/O multiplexing using a single `poll()` call
- NGINX-style configuration parser with location blocks and inheritance
- Support for `GET`, `POST`, and `DELETE` HTTP methods with configurable body size limits
- Static file serving, directory listing (`autoindex`), and custom error pages
- CGI execution (Python, shell scripts) with 7-second timeout watchdog
- File uploads via `multipart/form-data` and raw body
- Virtual hosting: multiple server blocks on the same port, matched by `Host` header
- Path traversal protection, duplicate header detection, XSS-safe autoindex

---

### Instructions

**Prerequisites**
- A C++98-compatible compiler (`g++` or `clang++`)
- A UNIX-like system (Linux or macOS)
- Python 3 (for CGI scripts)

**Installation**
```bash
git clone <repository-url>
cd webserv
make
```
Available Makefile rules: `all`, `clean`, `fclean`, `re`.

**Usage**
```bash
./webserv conf/default.conf
```

Then open **http://localhost:8080** in a browser to access the interactive demo page.

---

### Demo Page & Tests 

The server ships with a built-in demo page at `http://localhost:8080` that lets you test every feature interactively. Each test also shows the equivalent `curl` command so the evaluator can reproduce it in a terminal.

#### Features covered

| Section | What it tests |
|---------|--------------|
| **GET — Static Files** | Fetches `/`, `/about.html`, `/features.html`; links to autoindex on `/images/` and `/upload/` |
| **POST — Upload** | Multipart file upload to `/upload`, saved to `./www/upload/` |
| **DELETE** | Deletes an uploaded file; filename auto-filled after upload |
| **CGI GET** | `GET /cgi-bin/test.py?key=value` — shows all CGI env variables |
| **CGI POST** | `POST /cgi-bin/test.py` with a text body — shows `BODY RECU` in the table |
| **Non-Blocking CGI — Auto Test** | Fires `infini.py` (infinite loop) then 400 ms later sends `GET /`. The static page responds in < 50 ms while the CGI is still pending, proving the server is not blocked. The CGI is killed after the 7-second watchdog → **504**. |
| **Non-Blocking CGI — Manual** | Individual buttons for `infini.py`, `sleep.py` (both → 504), `bad_cgi.py` (malformed output → 500), `hello.py` (→ 200) |
| **Error Handling** | 404, 413 (sends 11 MB > 10 MB limit), 405, 403 |
| **Redirect** | `GET /old-page` → 301 → `/index.html` |

#### Quick curl reference

```bash
# Static files
curl -sv http://localhost:8080/
curl -sv http://localhost:8080/about.html
curl -sv http://localhost:8080/features.html

# Autoindex
curl -sv http://localhost:8080/images/
curl -sv http://localhost:8080/upload/

# Upload then delete
echo "Hello webserv!" > /tmp/test.txt
curl -sv -F "file=@/tmp/test.txt" http://localhost:8080/upload/test.txt
curl -sv -X DELETE http://localhost:8080/upload/test.txt

# CGI
curl -sv "http://localhost:8080/cgi-bin/test.py?name=evaluator&project=webserv"
curl -sv -X POST -H "Content-Type: text/plain" -d "Hello!" http://localhost:8080/cgi-bin/test.py

# Non-blocking proof (open 2 terminals)
# Terminal 1 — blocks ~7s then gets 504:
curl -sv http://localhost:8080/cgi-bin/infini.py
# Terminal 2 — responds immediately even while terminal 1 is waiting:
curl -sv http://localhost:8080/

# Error pages
curl -sv http://localhost:8080/does-not-exist                      # 404
curl -sv -X DELETE http://localhost:8080/html/index.html           # 405
curl -sv -X DELETE http://localhost:8080/upload/                   # 403
dd if=/dev/zero bs=1M count=11 2>/dev/null | \
  curl -sv -X POST -H "Content-Type: application/octet-stream" \
       -H "Content-Length: 11534336" --data-binary @- \
       http://localhost:8080/upload/bigfile.bin                    # 413

# Redirect
curl -sv  http://localhost:8080/old-page    # shows 301
curl -svL http://localhost:8080/old-page    # follows to /index.html
```

---

### Static Pages

Two standalone HTML pages are available via `GET`:

| URL | Description |
|-----|-------------|
| `/about.html` | Project overview: non-blocking I/O, CGI, uploads, virtual hosting |
| `/features.html` | Full feature table with implementation details |

> **Note:** All HTML pages (`index.html`, `about.html`, `features.html`, and all error pages) were generated with the assistance of **Claude (Anthropic)**.

---

### Resources

**Documentation & references**
- [RFC 1945 — HTTP/1.0 specification](https://datatracker.ietf.org/doc/html/rfc1945)
- [HTTP Tutorial by L.M. Garshol](https://www.garshol.priv.no/download/text/http-tut.html)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [NGINX Documentation](https://nginx.org/en/docs/)
- [The Linux `poll()` man page](https://man7.org/linux/man-pages/man2/poll.2.html)

---

### Use of AI

AI assistance (Claude, Anthropic) was used during this project for the following tasks:

- **Debugging**: diagnosing a `std::vector` out-of-bounds crash in the `poll()` event loop caused by iterator invalidation after erasing elements mid-iteration
- **CGI integration**: identifying a missing `cgi_ext`/`cgi_path` configuration causing Python scripts to be served as static files instead of being executed
- **HTML pages**: all front-end pages (`index.html`, `about.html`, `features.html`) were partially generated by Claude
