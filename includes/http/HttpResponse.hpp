#ifndef HTTPRESPONSE_HPP
# define HTTPRESPONSE_HPP

# include <iostream>
# include <map>
# include <sstream>
# include <string>

class HttpResponse
{
  public:
	HttpResponse();
	void setStatus(int code, const std::string &message);
	void addHeader(const std::string &key, const std::string &value);
	void setBody(const std::string &body);
	std::string toString() const;

  private:
	int _status_code;
	std::string _status_message;
	std::map<std::string, std::string> _header;
	std::string _body;
};

#endif
