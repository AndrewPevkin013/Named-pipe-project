#include <iostream>
#include <string>

#ifdef _WIN32
	#include <windows.h>
	typedef HANDLE PipeType;
    #define INVALID_PIPE_VALUE INVALID_HANDLE_VALUE
#else
	#include <fcntl.h>
	#include <unistd.h>
	#include <sys/stat.h>

	typedef int PipeType;
    #define INVALID_PIPE_VALUE -1
#endif

bool connect(PipeType pipe) {
	if (pipe == INVALID_PIPE_VALUE) {
		std::cout << "Faild to make connection on Named pipe!" << std::endl;
		#ifdef _WIN32
			system("pause");
		#endif
        return false;
	}
	return true;
}

void send(std::string msg, PipeType pipe) {
  	#ifdef _WIN32
    	DWORD bytes_written = 0;
    	DWORD msg_length = static_cast<DWORD>(msg.length() + 1);
    	BOOL result = WriteFile(
    	    pipe,
    	    msg.c_str(),
    	    msg_length,
    	    &bytes_written,
    	    NULL
    	);
    	if (!result) {
    	    std::cout << "Write failed!" << std::endl;
    	    return;
    	}
		std::cout << "Number of bytes sent: " << bytes_written << std::endl;
	#else
  		ssize_t bytes_written = write(pipe, msg.c_str(), msg.length() + 1);
        if (bytes_written < 0) {
          	std::cout << "Write failed!" << std::endl;
            return;
        }
        std::cout << "Number of bytes sent: " << bytes_written << std::endl;
    #endif
}

void closePipe(PipeType pipe) {
	#ifdef _WIN32
    	CloseHandle(pipe);
	#else
		::close(pipe);
	#endif
    std::cout << "Closing pipe. Done!" << std::endl;
}

int main(int argc, const char **argv) {
	PipeType pipe;

	#ifdef _WIN32
		pipe = CreateFile(
        	"\\\\.\\pipe\\NamedPipeTest",
        	GENERIC_WRITE,
        	FILE_SHARE_READ,
        	NULL,
        	OPEN_EXISTING,
        	FILE_ATTRIBUTE_NORMAL,
        	NULL
    	);
	#else
		const char* fifo_path = "/tmp/myfifo";
		pipe = open(fifo_path, O_WRONLY);
		if (pipe == INVALID_PIPE_VALUE) {
			std::cout << "Pipe doesn't exist." << std::endl;
			return 1;
		}
	#endif

	if (pipe == INVALID_PIPE_VALUE) {
		std::cout << "Failed to create pipe" << std::endl;
		#ifdef _WIN32
			system("pause");
		#endif
		return 1;
	}
    
	if (!connect(pipe)) {
		#ifdef _WIN32
	        system("pause");
		#endif
		return 1;
	}

   	std::cout << "Enter your message: ";
	std::string msg;
	std::getline(std::cin, msg);
    while (true) {
      	send(msg, pipe);
      	if (msg == ":q") {
            break;
      	}
		std::cout << "Enter your message: ";
		std::getline(std::cin, msg);
    }

    closePipe(pipe);
	#ifdef _WIN32
	    system("pause");
	#endif
	return 0;
}