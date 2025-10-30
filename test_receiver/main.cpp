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

const size_t BUFFER_SIZE = 1024;

bool receive(PipeType pipe) {
	char buf[BUFFER_SIZE];
    #ifdef _WIN32
        DWORD bytesRead = 0;
        BOOL is_read = ReadFile(
            pipe,
            buf,
            BUFFER_SIZE - 1,
            &bytesRead,
            NULL
        );
    #else
        ssize_t bytesRead = read(pipe, buf, BUFFER_SIZE - 1);
        bool is_read = (bytesRead > 0);
    #endif


    if (is_read && bytesRead > 0) {
        buf[bytesRead] = '\0';
        std::cout << "Bytes: " << bytesRead << " Message: " << buf << std::endl;
        if (std::string(buf) == ":q") {
        	return true;
        }
    }
    return false;
}

void closePipe(PipeType pipe) {
    #ifdef _WIN32
        CloseHandle(pipe);
    #else
        ::close(pipe);
        unlink("/tmp/myfifo");
    #endif
  	std::cout << "Closed" << std::endl;
}

int main(int argc, const char **argv) {
	std::cout << "Connecting to pipe..." << std::endl;
    PipeType pipe;

    #ifdef _WIN32
        pipe = CreateNamedPipe(
            "\\\\.\\pipe\\NamedPipeTest",
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            0, 0, 0,
            NULL
        );
    #else
        const char* fifo_path = "/tmp/myfifo";
        if (mkfifo(fifo_path, 0666) == -1) {
            std::cout << "FIFO already exist: " << errno << std::endl;
        }
        pipe = open(fifo_path, O_RDONLY);
    #endif

    if (pipe == INVALID_PIPE_VALUE) {
    	std::cout << "Failed to open pipe." << std::endl;
        #ifdef _WIN32
            system("pause");
        #endif
        return 1;
    }

    std::cout << "Reading message from pipe..." << std::endl;

    bool is_chating_end = false;
    while (!is_chating_end) {
        is_chating_end = receive(pipe);
    }

	closePipe(pipe);
    #ifdef _WIN32
        system("pause");
    #endif
	return 0;
}