#ifndef __DEV_HANDLER_HPP__
#define __DEV_HANDLER_HPP__

#include <iostream>

#include <fcntl.h>
#include <unistd.h>

class dev_handler {
public:
	enum io_method_ {
		    IO_METHOD_READ,
		    IO_METHOD_MMAP,
		    IO_METHOD_USERPTR,
	};

private:
	int fd;
	int io_method;
	int fps;
	bool is_dev;

public:
	dev_handler(int _io_method = -1, int _fps = -1) {
		fd = -1;
		io_method = _io_method;
		fps = _fps;
		is_dev = true;
	}

private:
	bool open_device(const char * path){
        fd = open(path, O_RDONLY);
        if(fd == -1)
            return false;
        return true;
    }

    void close_frame_file(){
    	if(fd != -1)
    		close(fd);
    }

public:
	bool init_frame_file(const char * path){
		return open_device(path);
	}

    bool read_frame_file(void * dest){
        int rdsz_ = read(fd, (unsigned char *)(dest),
	        sizeof(dest) / sizeof(char));
    	if(rdsz_ < 0x680) {
    	    std::cout << "A frame did not reach its full size.\n";
    	    return false;
    	}
    	return true;
    }
};

#endif // __DEV_HANDLER_HPP__
