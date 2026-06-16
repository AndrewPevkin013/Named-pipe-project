#pragma once

#include <string>

inline std::string make_pipe_name(const std::string& name)
{
#ifdef _WIN32
    return "\\\\.\\pipe\\space_" + name;
#else
    return "/tmp/ipc_transport/" + name;
#endif
}