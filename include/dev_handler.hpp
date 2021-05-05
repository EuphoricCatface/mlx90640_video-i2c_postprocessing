#ifndef __DEV_HANDLER_HPP__
#define __DEV_HANDLER_HPP__

#include <iostream>

#include <fcntl.h>
#include <unistd.h>

class dev_handler {
private:
	int ram_fd;

public:
	dev_handler() {ram_fd = -1;}

private:
	bool open_frame_file(const char * path){
        ram_fd = open(path, O_RDONLY);
        if(ram_fd == -1)
            return false;
        return true;
    }

    void close_frame_file(){
    	if(ram_fd != -1)
    		close(ram_fd);
    }

public:
	bool init_frame_file(const char * path){
		return open_frame_file(path);
	}

    bool read_frame_file(void * dest){
        int rdsz_ = read(ram_fd, (unsigned char *)(dest),
	        sizeof(dest) / sizeof(char));
    	if(rdsz_ < 0x680) {
    	    std::cout << "A frame did not reach its full size.\n";
    	    return false;
    	}
    	return true;
    }
};

#endif // __DEV_HANDLER_HPP__
