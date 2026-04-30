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

bool RequestHandler::findLocation(const std::string &path,
	const ServerConfig &config, Location &result)
{
	size_t	best_length;
	size_t	loc_len;

	best_length = 0;
	for (size_t i = 0; i < config.locations.size(); ++i)
	{
		if (path.find(config.locations[i].path) == 0)
		{
			loc_len = config.locations[i].path.length();
			if (path.length() == loc_len || path[loc_len] == '/' || config.locations[i].path == "/")
			{
				if (loc_len > best_length)
				{
					best_length = loc_len;
					result = config.locations[i];
				}
			}
		}
	}
	return (best_length == 0 ? false : true);
}

// bool RequestHandler::findLocation(const std::string &path, const ServerConfig &config, Location &result) //debug
// {
//     size_t best_length = 0;
//     for (size_t i = 0; i < config.locations.size(); ++i)
//     {
//         std::cerr << "comparing: '" << path << "' with '" << config.locations[i].path << "'" << std::endl;
//         std::cerr << "find result: " << path.find(config.locations[i].path) << std::endl;

//         if (path.find(config.locations[i].path) == 0)
//         {
//             size_t loc_len = config.locations[i].path.length();
//             std::cerr << "loc_len: " << loc_len << " path_len: " << path.length() << std::endl;
//             std::cerr << "next char: '" << path[loc_len] << "'" << std::endl;

//             if (path.length() == loc_len || path[loc_len] == '/' || config.locations[i].path == "/")
//             {
//                 std::cerr << "MATCH" << std::endl;
//                 if (loc_len > best_length)
//                 {
//                     best_length = loc_len;
//                     result = config.locations[i];
//                 }
//             }
//         }
//     }
//     return (best_length == 0 ? false : true);
// }


HttpResponse RequestHandler::handleRequest(const HttpRequest &request,
	const ServerConfig &config)
{
	Location		loc;
	HttpResponse	res;

    // std::cerr << "body size: " << request.getBody().size() << std::endl; //debug
    // std::cerr << "max body: " << config.client_max_body_size << std::endl; //debug
    if (request.getErrorCode() != 0)
    {
        return (buildErrorResponse(request.getErrorCode()));
    }

	if (request.getBody().size() > config.client_max_body_size)
		return (buildErrorResponse(413));

    // std::cerr << "path: " << request.getPath() << std::endl; //debug
    // std::cerr << "locations size: " << config.locations.size() << std::endl; //debug
	if (!findLocation(request.getPath(), config, loc))
    {
        // std::cerr << "location not found" << std::endl; //debug
        return (buildErrorResponse(404));
    }
	// std::cerr << "location found: " << loc.path << std::endl; //debug
    if (!loc.return_url.empty())
	{
		res.setStatus(301, "Moved Permanently");
		res.addHeader("Location", loc.return_url);
		return (res);
	}
	if (std::find(loc.methods.begin(), loc.methods.end(),
			request.getMethod()) == loc.methods.end())
		return (buildErrorResponse(405));
	if (!loc.cgi_ext.empty() && (request.getPath().substr(request.getPath().find_last_of(".")) == loc.cgi_ext))
		return (handleCgi(request, loc));
	else
	{
		if (request.getMethod() == "GET")
			return (handleGet(request, loc));
		else if (request.getMethod() == "POST")
			return (handlePost(request, loc));
		else if (request.getMethod() == "DELETE")
			return (handleDelete(request, loc));
	}

	return (buildErrorResponse(501)); // Not Implemented
}

HttpResponse RequestHandler::handleCgi(const HttpRequest &req, const Location &loc) //gerer les timeouts
{
    int inPipe[2];
    int outPipe[2];

    if (pipe(inPipe) == -1 || pipe(outPipe) == -1)
        return buildErrorResponse(500);

    pid_t pid = fork();
    if (pid < 0)
        return buildErrorResponse(500);

    if (pid == 0)
    {
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);

        close(inPipe[1]);
        close(outPipe[0]);
        close(inPipe[0]);
        close(outPipe[1]);

		char *envp[7];
		std::string s0 = "REQUEST_METHOD=" + req.getMethod();
		std::string s1 = "QUERY_STRING="   + req.getQueryString();
		std::string s2 = "CONTENT_LENGTH=" + Utils::intToString(req.getBody().size());
		std::string content_type_val = "";
		std::map<std::string, std::string>::const_iterator it = req.getHeader().find("content-type");
		if (it != req.getHeader().end())
			content_type_val = it->second;
		std::string s3 = "CONTENT_TYPE=" + content_type_val;
		std::string s4 = "SCRIPT_NAME="    + loc.root + req.getPath();
		std::string s5 = "PATH_INFO="      + loc.root + req.getPath();


		envp[0] = (char *)s0.c_str();
		envp[1] = (char *)s1.c_str();
		envp[2] = (char *)s2.c_str();
		envp[3] = (char *)s3.c_str();
		envp[4] = (char *)s4.c_str();
		envp[5] = (char *)s5.c_str();
		envp[6] = NULL;
		std::string script_path = loc.root + req.getPath();

        // EXEC
        char *argv[3];
        argv[0] = (char *)loc.cgi_path.c_str();   // ex: /usr/bin/python3
        argv[1] = (char *)script_path.c_str();    // script
        argv[2] = NULL;

        execve(argv[0], argv, envp);

        // si exec échoue
        perror("execve");
        exit(1);
    }

    // PARENT

    close(inPipe[0]);
    close(outPipe[1]);

    // envoyer body (POST)
    if (!req.getBody().empty())
        write(inPipe[1], req.getBody().c_str(), req.getBody().size()); //integrer poll pour éviter blocage si le script ne lit pas tout de suite
    close(inPipe[1]);

    // lire output
    char buffer[1024];
    std::string output;
    int bytes;

    while ((bytes = read(outPipe[0], buffer, sizeof(buffer))) > 0) //integrer poll pour éviter blocage si le script ne lit pas tout de suite
        output.append(buffer, bytes);

    close(outPipe[0]);

    waitpid(pid, NULL, 0);

    // PARSE CGI OUTPUT
    HttpResponse res;

    size_t pos = output.find("\r\n\r\n");
    if (pos == std::string::npos)
        return buildErrorResponse(500);

    std::string headers = output.substr(0, pos);
    std::string body = output.substr(pos + 4);

    std::istringstream stream(headers);
    std::string line;

    while (std::getline(stream, line))
    {
        if (line.empty())
            continue;

        size_t sep = line.find(":");
        if (sep != std::string::npos)
        {
            std::string key = line.substr(0, sep);
            std::string value = line.substr(sep + 2); // skip ": "
            res.addHeader(key, value);
        }
    }

    res.setStatus(200, "OK");
	res.addHeader("Content-Length", Utils::intToString(body.size()));
    res.setBody(body);

    return res;
}

HttpResponse RequestHandler::handleGet(const HttpRequest &req,
	const Location &loc)
{
	std::string file_path = loc.root + req.getPath();
    // std::cerr << "file_path: " << file_path << std::endl; //debug
    // struct stat s;
    // int stat_result = stat(file_path.c_str(), &s);
    // std::cerr << "stat result: " << stat_result << std::endl;
	if (file_path[file_path.size() - 1] == '/')
	{
		file_path += loc.index.empty() ? "index.html" : loc.index;
	}
	struct stat s;
	if (stat(file_path.c_str(), &s) == -1)
	{
		return (buildErrorResponse(404));
	}
	if (S_ISDIR(s.st_mode))
	{
		if (loc.autoindex)
		{
			std::string body = "<html><body><h1>Directory listing for " + req.getPath() + "</h1><ul>";
			DIR *dir = opendir(file_path.c_str());
			if (dir)
			{
				struct dirent *entry;
				while ((entry = readdir(dir)) != NULL)
				{
					std::string name = entry->d_name;
					if (name != "." && name != "..")
					{
						body += "<li><a href=\"" + name + "\">" + name
							+ "</a></li>";
					}
				}
				closedir(dir);
			}
			body += "</ul></body></html>";
			HttpResponse res;
			res.setStatus(200, "OK");
			res.addHeader("Content-Type", "text/html");
			res.setBody(body);
			return (res);
		}
		else
		{
			return (buildErrorResponse(403));
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
		return (buildErrorResponse(403));
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

HttpResponse RequestHandler::handleDelete(const HttpRequest &req,const Location &loc)
{
    std::string file_path = loc.root + req.getPath();
    struct stat s;
    if (stat(file_path.c_str(), &s) == -1)
    {
        return (buildErrorResponse(404));
    }
    if (S_ISDIR(s.st_mode))
    {
        return (buildErrorResponse(403));
    }
    if (remove(file_path.c_str()) != 0)
    {
        return (buildErrorResponse(500));
    }
    HttpResponse res;
    res.setStatus(204, "No Content");
    return (res);
}

HttpResponse RequestHandler::buildErrorResponse(int code)
{
    HttpResponse res;
    std::string message;
    switch (code)
    {
    case 400:
        message = "Bad Request";
        break;
    case 403:
        message = "Forbidden";
        break;
    case 404:
        message = "Not Found";
        break;
    case 405:
        message = "Method Not Allowed";
        break;
    case 413:
        message = "Payload Too Large";
        break;
    case 500:
        message = "Internal Server Error";
        break;
    case 501:
        message = "Not Implemented";
        break;
    case 505:
        message = "HTTP Version Not Supported";
        break;
    default:
        message = "Error";
    }
    res.setStatus(code, message);
    res.addHeader("Content-Type", "text/html");
    std::string body = "<html><body><h1>" + Utils::intToString(code) + " " + message + "</h1></body></html>";
    res.addHeader("Content-Length", Utils::intToString(body.size()));
    res.setBody(body);
    return (res);
}

HttpResponse RequestHandler::handlePost(const HttpRequest &req, const Location &loc)
{
    if (loc.upload_store.empty())
        return buildErrorResponse(403);

    std::map<std::string, std::string> headers = req.getHeader();
    if (headers.find("content-type") == headers.end())
        return buildErrorResponse(400);

    std::string content_type = headers["content-type"];

    if (content_type.find("multipart/form-data") == std::string::npos)
    {
        std::string filename = req.getPath().substr(req.getPath().find_last_of("/") + 1);
        std::string file_path = loc.upload_store + "/" + filename;
        std::ofstream file(file_path.c_str(), std::ios::binary);
        if (!file.is_open())
            return buildErrorResponse(500);
        file << req.getBody();
        file.close();
        HttpResponse res;
        res.setStatus(201, "Created");
        return res;
    }

    size_t boundary_pos = content_type.find("boundary=");
    if (boundary_pos == std::string::npos)
        return buildErrorResponse(400);

    std::string boundary = "--" + content_type.substr(boundary_pos + 9);
    std::string end_boundary = boundary + "--";

    const std::string &body = req.getBody();

    size_t pos = body.find(boundary);
    if (pos == std::string::npos)
        return buildErrorResponse(400);

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
        std::string filename = part_headers.substr(filename_pos, filename_end - filename_pos);

        std::string file_path = loc.upload_store + "/" + filename;

        std::ofstream file(file_path.c_str(), std::ios::binary);
        if (!file.is_open())
            return buildErrorResponse(500);

        size_t data_start = headers_end + 4;

        size_t next_boundary = body.find(boundary, data_start);
        if (next_boundary == std::string::npos)
        {
            break;
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
