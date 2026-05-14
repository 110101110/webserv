#include "http/RequestHandler.hpp"
#include "utils/Utils.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vector>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "utils/Logger.hpp"


static bool _matchTildePattern(const std::string &pattern, const std::string &path)
{
    std::string ext;
    for (size_t i = 0; i < pattern.size(); i++)
    {
        if (pattern[i] == '$' && i + 1 == pattern.size())
            break;
        if (pattern[i] == '\\' && i + 1 < pattern.size())
            ext += pattern[++i];
        else
            ext += pattern[i];
    }
    if (ext.empty() || ext.size() > path.size())
        return false;
    return path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

// ── Path traversal guard ──────────────────────────────────────────────────────
// Normalise un chemin en résolvant les composants "..".
// Retourne "" si le chemin tente de sortir de la racine (traversal détecté).
static std::string _normalizePath(const std::string &path)
{
    std::vector<std::string> parts;
    size_t i = 0;

    while (i <= path.size())
    {
        size_t slash = path.find('/', i);
        std::string seg = (slash == std::string::npos)
            ? path.substr(i)
            : path.substr(i, slash - i);
        i = (slash == std::string::npos) ? path.size() + 1 : slash + 1;

        if (seg.empty() || seg == ".")
            continue;
        if (seg == "..")
        {
            if (parts.empty())
                return ""; // tentative de sortir au-dessus de la racine
            parts.pop_back();
        }
        else
            parts.push_back(seg);
    }

    std::string result = "/";
    for (size_t j = 0; j < parts.size(); j++)
        result += (j > 0 ? "/" : "") + parts[j];
    return result;
}

// Retourne root + req_path si le chemin est sûr, chaîne vide si traversal détecté.
static std::string _resolvePath(const std::string &root,
                                 const std::string &req_path)
{
    if (_normalizePath(req_path) == "")
        return "";
    return root + req_path;
}

bool RequestHandler::findLocation(const std::string &path,
	const ServerConfig &config, Location &result)
{
	// Passe 1 : locations ~ (extension matching), premier match gagne
	for (size_t i = 0; i < config.locations.size(); ++i)
	{
		const std::string &lpath = config.locations[i].path;
		// LOG_DEBUG(lpath);
		if (lpath.size() > 2 && lpath[0] == '~' && lpath[1] == ' ')
		{
			if (_matchTildePattern(lpath.substr(2), path))
			{
				result = config.locations[i];
				return true;
			}
		}
	}

	// Passe 2 : locations préfixe, longest match gagne
	size_t best_length = 0;
	for (size_t i = 0; i < config.locations.size(); ++i)
	{
		const std::string &lpath = config.locations[i].path;
		if (lpath.size() > 2 && lpath[0] == '~' && lpath[1] == ' ')
			continue;
		if (path.find(lpath) == 0)
		{
			size_t loc_len = lpath.length();
			if (path.length() == loc_len || path[loc_len] == '/' || lpath == "/")
			{
				if (loc_len > best_length)
				{
					best_length = loc_len;
					result = config.locations[i];
				}
			}
		}
	}
	return (best_length > 0);
}


HttpResponse RequestHandler::handleRequest(const HttpRequest &request,
	const ServerConfig &config, CgiContext *cgi_out)
{
	Location		loc;
	HttpResponse	res;

	int err = request.getErrorCode();
	if (err != 0)
		return buildErrorResponse(err > 0 ? err : 400, config);

	if (request.getBody().size() > config.client_max_body_size)
		return buildErrorResponse(413, config);

	if (!findLocation(request.getPath(), config, loc))
		return buildErrorResponse(404, config);

	if (!loc.return_url.empty())
	{
		int code = 301;
		std::string url = loc.return_url;
		size_t space = url.find(' ');
		if (space != std::string::npos)
		{
			std::string code_str = url.substr(0, space);
			if (Utils::isNumber(code_str))
			{
				code = std::atoi(code_str.c_str());
				url = url.substr(space + 1);
			}
		}
		res.setStatus(code, _getStatusMessage(code));
		res.addHeader("Location", url);
		return res;
	}
	if (std::find(loc.methods.begin(), loc.methods.end(),
			request.getMethod()) == loc.methods.end())
		return buildErrorResponse(405, config);

	size_t dot_pos = request.getPath().find_last_of(".");
	if (!loc.cgi_ext.empty() && dot_pos != std::string::npos &&
		request.getPath().substr(dot_pos) == loc.cgi_ext)
	{
		if (cgi_out == NULL)
			return buildErrorResponse(500, config);
		if (!launchCgi(request, loc, config, *cgi_out))
			return buildErrorResponse(500, config);
		return res; // réponse ignorée, cgi_out->isValid() == true
	}

	if (request.getMethod() == "GET")
		return handleGet(request, loc, config);
	if (request.getMethod() == "POST")
		return handlePost(request, loc, config);
	if (request.getMethod() == "DELETE")
		return handleDelete(request, loc, config);

	return buildErrorResponse(501, config);
}

// Lance le CGI de façon non-bloquante :
// - fork + exec
// - pipe_out mis en O_NONBLOCK (le parent lit via poll)
// - body POST écrit de façon synchrone (taille limitée par client_max_body_size)
// - remplit ctx, retourne true si succès
bool RequestHandler::launchCgi(const HttpRequest &req, const Location &loc,
	const ServerConfig &config, CgiContext &ctx)
{
	(void)config;
	int inPipe[2];
	int outPipe[2];

	if (pipe(inPipe) == -1)
		return false;
	if (pipe(outPipe) == -1)
	{
		close(inPipe[0]); close(inPipe[1]);
		return false;
	}

	// Le parent lit outPipe[0] via poll → non-bloquant
	fcntl(outPipe[0], F_SETFL, O_NONBLOCK);

	pid_t pid = fork();
	if (pid < 0)
	{
		close(inPipe[0]); close(inPipe[1]);
		close(outPipe[0]); close(outPipe[1]);
		return false;
	}

	if (pid == 0)
	{
		dup2(inPipe[0], STDIN_FILENO);
		dup2(outPipe[1], STDOUT_FILENO);
		close(inPipe[0]); close(inPipe[1]);
		close(outPipe[0]); close(outPipe[1]);
		for (int fd = 3; fd < 1024; fd++)
			close(fd);

		std::string s0 = "REQUEST_METHOD=" + req.getMethod();
		std::string s1 = "QUERY_STRING="   + req.getQueryString();
		std::string s2 = "CONTENT_LENGTH=" + Utils::intToString(req.getBody().size());
		std::string content_type_val;
		std::map<std::string, std::string>::const_iterator it =
			req.getHeader().find("content-type");
		if (it != req.getHeader().end())
			content_type_val = it->second;
		std::string s3 = "CONTENT_TYPE="    + content_type_val;
		std::string script_path = loc.root + req.getPath();
		std::string s4 = "SCRIPT_FILENAME=" + script_path;
		std::string s5 = "PATH_INFO="       + req.getPath();
		std::string s6 = "SCRIPT_NAME="     + req.getPath();

		char *envp[8];
		envp[0] = (char *)s0.c_str();
		envp[1] = (char *)s1.c_str();
		envp[2] = (char *)s2.c_str();
		envp[3] = (char *)s3.c_str();
		envp[4] = (char *)s4.c_str();
		envp[5] = (char *)s5.c_str();
		envp[6] = (char *)s6.c_str();
		envp[7] = NULL;

		char *argv[3];
		argv[0] = (char *)loc.cgi_path.c_str();
		argv[1] = (char *)script_path.c_str();
		argv[2] = NULL;

		execve(argv[0], argv, envp);
		exit(1);
	}

	// Parent
	close(inPipe[0]);
	close(outPipe[1]);

	if (!req.getBody().empty())
	{
		const char *ptr = req.getBody().c_str();
		size_t remaining = req.getBody().size();
		while (remaining > 0)
		{
			ssize_t written = write(inPipe[1], ptr, remaining);
			if (written <= 0)
				break;
			ptr += written;
			remaining -= written;
		}
	}
	close(inPipe[1]);

	ctx.pid        = pid;
	ctx.pipe_out   = outPipe[0];
	ctx.start_time = time(NULL);
	return true;
}

// static
// Parse la sortie brute CGI (headers\r\n\r\nbody) en HttpResponse.
HttpResponse RequestHandler::parseCgiOutput(const std::string &raw)
{
	HttpResponse res;

	// Supporte \r\n\r\n (standard) et \n\n (certains scripts Unix)
	size_t sep  = raw.find("\r\n\r\n");
	size_t skip = 4;
	if (sep == std::string::npos)
	{
		sep  = raw.find("\n\n");
		skip = 2;
	}
	if (sep == std::string::npos)
	{
		res.setStatus(500, "Internal Server Error");
		std::string body = "<html><body><h1>500 Internal Server Error</h1>"
		                   "<p>CGI output malformed</p></body></html>";
		res.addHeader("Content-Type", "text/html");
		res.addHeader("Content-Length", Utils::intToString(body.size()));
		res.setBody(body);
		return res;
	}

	std::string hdr_raw = raw.substr(0, sep);
	std::string body    = raw.substr(sep + skip);

	std::istringstream stream(hdr_raw);
	std::string line;
	while (std::getline(stream, line))
	{
		// Retire le \r final (getline ne le retire pas)
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.resize(line.size() - 1);
		if (line.empty()) continue;
		size_t colon = line.find(':');
		if (colon != std::string::npos)
		{
			std::string key = line.substr(0, colon);
			std::string val = line.substr(colon + 1);
			size_t s = val.find_first_not_of(' ');
			if (s != std::string::npos) val = val.substr(s);
			res.addHeader(key, val);
		}
	}
	res.setStatus(200, "OK");
	res.addHeader("Content-Length", Utils::intToString(body.size()));
	res.setBody(body);
	return res;
}

HttpResponse RequestHandler::handleGet(const HttpRequest &req,
	const Location &loc, const ServerConfig &config)
{
	std::string file_path = _resolvePath(loc.root, req.getPath());
	if (file_path.empty())
		return buildErrorResponse(403, config);
	if (file_path[file_path.size() - 1] == '/')
	{
		file_path += loc.index.empty() ? "index.html" : loc.index;
	}
	struct stat s;
	if (stat(file_path.c_str(), &s) == -1)
	{
		return buildErrorResponse(404, config);
	}
	if (S_ISDIR(s.st_mode))
	{
        //fix: adding '/' at the end of path to find an index.html if it exist
        std::string index_path = file_path;
        if (index_path[index_path.size() - 1] != '/'){
            index_path += '/';
        }
        index_path += loc.index.empty() ? "index.html" : loc.index;
        struct stat index_stat;
        if (stat(index_path.c_str(), &index_stat) == 0 && S_ISREG(index_stat.st_mode)){
            file_path = index_path;
            s = index_stat;
        }
        else if (loc.autoindex)
		{
			DIR *dir = opendir(file_path.c_str());
			if (!dir)
				return buildErrorResponse(500, config);
			std::string body = "<html><body><h1>Directory listing for "
				+ Utils::htmlEscape(req.getPath()) + "</h1><ul>";
			struct dirent *entry;
			while ((entry = readdir(dir)) != NULL)
			{
				std::string name = entry->d_name;
				if (name != "." && name != "..")
				{
					std::string escaped = Utils::htmlEscape(name);
					body += "<li><a href=\"" + escaped + "\">" + escaped + "</a></li>";
				}
			}
			closedir(dir);
			body += "</ul></body></html>";
			HttpResponse res;
			res.setStatus(200, "OK");
			res.addHeader("Content-Type", "text/html");
			res.addHeader("Content-Length", Utils::intToString(body.size()));
			res.setBody(body);
			return res;
		}
		else
		{
			return buildErrorResponse(403, config);
		}
	}
	std::string content_type = "application/octet-stream";
	if (S_ISREG(s.st_mode))
	{
		std::string extension = file_path.substr(file_path.find_last_of(".")
				+ 1);
		if (extension == "html" || extension == "htm")
			content_type = "text/html";
		else if (extension == "jpg" || extension == "jpeg")
			content_type = "image/jpeg";
		else if (extension == "png")
			content_type = "image/png";
		else if (extension == "css")
			content_type = "text/css";
		else if (extension == "js")
			content_type = "application/javascript";
	}
	std::ifstream file(file_path.c_str(), std::ios::binary);
	if (!file.is_open())
		return buildErrorResponse(403, config);
	std::ostringstream ss;
	ss << file.rdbuf();
	std::string content = ss.str();
	HttpResponse res;
	res.setStatus(200, "OK");
	res.addHeader("Content-Type", content_type);
    res.addHeader("Content-Length", Utils::intToString(content.size()));
	res.setBody(content);
	return (res);
}

HttpResponse RequestHandler::handleDelete(const HttpRequest &req,const Location &loc, const ServerConfig &config)
{
    std::string file_path = _resolvePath(loc.root, req.getPath());
    if (file_path.empty())
        return buildErrorResponse(403, config);
    struct stat s;
    if (stat(file_path.c_str(), &s) == -1)
    {
        return buildErrorResponse(404, config);
    }
    if (S_ISDIR(s.st_mode))
    {
        return buildErrorResponse(403, config);
    }
    if (remove(file_path.c_str()) != 0)
    {
        return buildErrorResponse(500, config);
    }
    HttpResponse res;
    res.setStatus(204, "No Content");
    return (res);
}

std::string RequestHandler::_getStatusMessage(int code)
{
	switch (code) {
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Payload Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		default:  return "Error";
	}
}

HttpResponse RequestHandler::buildErrorResponse(int code, const ServerConfig &config)
{
	HttpResponse res;
	std::string message = _getStatusMessage(code);
	res.setStatus(code, message);

	// recherche page erreur perso
	std::map<int, std::string>::const_iterator it = config.error_pages.find(code);
	if (it != config.error_pages.end())
	{
		std::string error_path = config.root;
		if (!error_path.empty() && error_path[error_path.size() - 1] != '/')
			error_path += "/";
		error_path += it->second;

		std::ifstream file(error_path.c_str());
		if (file.is_open())
		{
			std::ostringstream ss;
			ss << file.rdbuf();
			std::string content = ss.str();
			res.addHeader("Content-Type", "text/html");
			res.addHeader("Content-Length", Utils::intToString(content.size()));
			res.setBody(content);
			return res;
		}
		LOG_WARNING("Custom error page not found: " + error_path);
	}

	// Fallback
	std::string body = "<html><head><title>" + Utils::intToString(code) + " " + message + "</title></head>";
	body += "<body style='font-family:sans-serif; text-align:center; padding-top:10%; background:#f4f4f4;'>";
	body += "<h1>" + Utils::intToString(code) + " " + message + "</h1><hr style='width:50%'>";
	body += "<p>webserv/1.0</p></body></html>";

	res.addHeader("Content-Type", "text/html");
	res.addHeader("Content-Length", Utils::intToString(body.size()));
	res.setBody(body);
	return res;
}


HttpResponse RequestHandler::handlePost(const HttpRequest &req, const Location &loc, const ServerConfig &config)
{
    if ( req.getMethod() == "POST" && loc.upload_store.empty())
        return buildErrorResponse(403, config);

    std::map<std::string, std::string> headers = req.getHeader();
    if (headers.find("content-type") == headers.end())
        return buildErrorResponse(400, config);

    std::string content_type = headers["content-type"];

    if (content_type.find("multipart/form-data") == std::string::npos)
    {
        std::string filename = req.getPath().substr(req.getPath().find_last_of("/") + 1);
        std::string file_path = _resolvePath(loc.upload_store, "/" + filename);
        if (file_path.empty())
            return buildErrorResponse(403, config);
        std::ofstream file(file_path.c_str(), std::ios::binary);
        if (!file.is_open())
            return buildErrorResponse(500, config);
        file << req.getBody();
        file.close();
        HttpResponse res;
        res.setStatus(201, "Created");
        return res;
    }

    size_t boundary_pos = content_type.find("boundary=");
    if (boundary_pos == std::string::npos)
        return buildErrorResponse(400, config);

    std::string boundary = "--" + content_type.substr(boundary_pos + 9);
    std::string end_boundary = boundary + "--";

    const std::string &body = req.getBody();

    size_t pos = body.find(boundary);
    if (pos == std::string::npos)
        return buildErrorResponse(400, config);

    while (pos != std::string::npos)
    {
        pos += boundary.length();

        if (body.substr(pos, 2) == "\r\n")
            pos += 2;

        size_t headers_end = body.find("\r\n\r\n", pos);
        if (headers_end == std::string::npos)
            break;

        std::string part_headers = body.substr(pos, headers_end - pos);

        size_t filename_pos = part_headers.find("filename=\"");
        if (filename_pos == std::string::npos)
        {
            pos = body.find(boundary, headers_end);
            continue;
        }

        filename_pos += 10;
        size_t filename_end = part_headers.find("\"", filename_pos);
        if (filename_end == std::string::npos)
        {
            pos = body.find(boundary, headers_end);
            continue;
        }
        std::string filename = part_headers.substr(filename_pos, filename_end - filename_pos);

        std::string file_path = _resolvePath(loc.upload_store, "/" + filename);
        if (file_path.empty())
            return buildErrorResponse(403, config);

        std::ofstream file(file_path.c_str(), std::ios::binary);
        if (!file.is_open())
            return buildErrorResponse(500, config);

        size_t data_start = headers_end + 4;

        size_t next_boundary = body.find(boundary, data_start);
        if (next_boundary == std::string::npos)
        {
            file.close();
            return buildErrorResponse(400, config);
        }

        size_t data_end = next_boundary - 2;

        file.write(body.c_str() + data_start, data_end - data_start);
        file.close();

        pos = next_boundary;
    }

    HttpResponse res;
    res.setStatus(201, "Created");
    return res;
}
