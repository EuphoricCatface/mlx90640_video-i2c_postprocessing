#ifndef __DEV_HANDLER_HPP__
#define __DEV_HANDLER_HPP__

#include <iostream>

#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#define BUF_COUNT 2

#define CLEAR(x) memset(&(x), 0, sizeof(x))

class dev_handler {
public:
	enum io_method_ {
	    IO_METHOD_READ,
	    IO_METHOD_MMAP
	};

private:
	int fd;
	int io_method;
	int fps;
	bool is_dev;

	bool open_;
	bool init;
	bool capturing;

	bool extended;
	// Based on whether the device output format is 26-line or 27-line.
	// Only using user-provided value when parsing raw file.
	// Overwritten while parsing v4l2 format.

	struct buffer {
        void   *start;
        size_t  length;
	};
	struct buffer *buffers;

public:
	dev_handler(int _io_method = -1, int _fps = -1, bool extended_format = false)
		: io_method(_io_method), fps(_fps), extended(extended_format) {
		fd = -1;
		is_dev = true;
		open_ = false;
		init = false;
		capturing = false;
	}
	~dev_handler() {
		if (is_dev) {
			if(capturing) stop_capturing();
			if(init) uninit_device();
		}
		if(open_) close_device();
	}

private: // basic tools
	int xioctl(int fh, int request, void *arg)
	{
        int r;

        do {
            r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
	}

	static void errno_exit(const char *s)
	{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
	}

private: // getting to work
	void open_device(const char * path);
    void init_v4l2_device(void);
    void init_mmap(void);

	void init_read(unsigned int buffer_size);

    bool read_raw(void * dest);
    int read_v4l2_frame(void * dest);

private: // cleanups
	void stop_capturing(void);
	void uninit_device(void);
	void close_device(void);

public:
	void init_frame_file(const char * path);
	void start_capturing(void);
    bool read_frame_file(void * dest);

    bool is_extended(void){
    	return extended;
    }
};

#endif // __DEV_HANDLER_HPP__
