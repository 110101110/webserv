#pragma once
#include <sys/types.h>
#include <string>
#include <ctime>

struct CgiContext {
    pid_t       pid;
    int         pipe_out;   // fd lecture sortie CGI (non-bloquant, surveillé par poll)
    int         client_fd;
    size_t      config_idx;
    std::string output;     // buffer de sortie CGI accumulé
    time_t      start_time;

    CgiContext() : pid(-1), pipe_out(-1), client_fd(-1), config_idx(0), start_time(0) {}
    bool isValid() const { return pipe_out != -1; }
};
