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

	struct buffer {
        void   *start;
        size_t  length;
	};
	struct buffer *buffers;

public:
	dev_handler(int _io_method = -1, int _fps = -1) {
		fd = -1;
		io_method = _io_method;
		fps = _fps;
		is_dev = true;
	}
	~dev_handler() {
		if (is_dev) {
			stop_capturing();
			uninit_device();
		}
		close_device();
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
	void open_device(const char * path){
        struct stat st;
        struct v4l2_format fmt;

        if (-1 == stat(path, &st)) {
            fprintf(stderr, "Cannot identify '%s': %d, %s\n",
             		path, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
            fprintf(stderr, "%s is not a device\n", path);
            is_dev = false;
        }

		if (is_dev)
        	fd = open(path, O_RDWR /* required */ | O_NONBLOCK, 0);
        else
        	fd = open(path, O_RDONLY);

        if (-1 == fd) {
            fprintf(stderr, "Cannot open '%s': %d, %s\n",
                    path, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        CLEAR(fmt);
        if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
            errno_exit("VIDIOC_G_FMT");

        printf("Video width: %d, height: %d\n",
        		fmt.fmt.pix.width,
        		fmt.fmt.pix.height);

        switch (io_method) {
        case IO_METHOD_READ:
                init_read(fmt.fmt.pix.sizeimage);
                break;

        case IO_METHOD_MMAP:
                init_mmap();
                break;
        }
    }

    void init_v4l2_device(){
        struct v4l2_capability cap;

    	if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
            if (EINVAL == errno) {
                fprintf(stderr, "Device is not a V4L2 device\n");
                exit(EXIT_FAILURE);
            } else {
                errno_exit("VIDIOC_QUERYCAP");
            }
        }

        switch (io_method) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                fprintf(stderr, "Device does not support read i/o\n");
                exit(EXIT_FAILURE);
            }
            break;

        case IO_METHOD_MMAP:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                fprintf(stderr, "Device does not support streaming i/o\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }

    void init_mmap(void)
	{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count = BUF_COUNT;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
            if (EINVAL == errno) {
                    fprintf(stderr, "Device does not support "
                             "memory mapping\n");
                    exit(EXIT_FAILURE);
            } else {
                    errno_exit("VIDIOC_REQBUFS");
            }
        }

        if (req.count < 2) {
            fprintf(stderr, "Insufficient buffer memory on device\n");
            exit(EXIT_FAILURE);
        }

        buffers = (buffer*) calloc(req.count, sizeof(*buffers));

        if (!buffers) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }

        for (unsigned int n_buffers = 0; n_buffers < req.count; ++n_buffers) {
            struct v4l2_buffer buf;

            CLEAR(buf);

            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = n_buffers;

            if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
                    errno_exit("VIDIOC_QUERYBUF");

            buffers[n_buffers].length = buf.length;
            buffers[n_buffers].start =
                    mmap(NULL /* start anywhere */,
                          buf.length,
                          PROT_READ | PROT_WRITE /* required */,
                          MAP_SHARED /* recommended */,
                          fd, buf.m.offset);

            if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit("mmap");
        }
	}

	void init_read(unsigned int buffer_size)
	{
        buffers = (buffer*)calloc(1, sizeof(*buffers));

        if (!buffers) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }

        buffers[0].length = buffer_size;
        buffers[0].start = malloc(buffer_size);

        if (!buffers[0].start) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
	}

    bool read_raw(void * dest){
    	int rdsz_ = read(fd, (unsigned char *)(dest),
		    sizeof(dest) / sizeof(char));
		if(rdsz_ < 0x680) {
		    std::cout << "A frame did not reach its full size.\n";
		    return false;
		}
		return true;
    }

    int read_v4l2_frame(void * dest){
    	struct v4l2_buffer buf;

        switch (io_method) {
        case IO_METHOD_READ:
            if (-1 == read(fd, buffers[0].start, buffers[0].length)) {
                switch (errno) {
                case EAGAIN:
                    return 0;

                case EIO:
                    /* Could ignore EIO, see spec. */

                    /* fall through */

                default:
                    errno_exit("read");
                }
            }

            //process_image(buffers[0].start, buffers[0].length);
			memcpy(dest, buffers[0].start, buffers[0].length);
            break;

        case IO_METHOD_MMAP:
            CLEAR(buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                case EAGAIN:
                        return 0;

                case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                default:
                        errno_exit("VIDIOC_DQBUF");
                }
            }

            assert(buf.index < BUF_COUNT);

            //process_image(buffers[buf.index].start, buf.bytesused);
			memcpy(dest, buffers[buf.index].start, buf.bytesused);
			//FIXME?: memcpy is probably throwing the benefit of mmap outta the window,
			//but let's consider it later...

            if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                    errno_exit("VIDIOC_QBUF");
            break;
        }

        return 1;
    }

private: // cleanups
	void stop_capturing(void)
	{
        enum v4l2_buf_type type;

        switch (io_method) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
                errno_exit("VIDIOC_STREAMOFF");
            break;
        }
	}

	void uninit_device(void)
	{
        unsigned int i;

        switch (io_method) {
        case IO_METHOD_READ:
            free(buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < BUF_COUNT; ++i)
                if (-1 == munmap(buffers[i].start, buffers[i].length))
                    errno_exit("munmap");
            break;
        }

        free(buffers);
	}

	void close_device(void)
	{
        if (-1 == close(fd))
            errno_exit("close");

        fd = -1;
	}

public:
	void init_frame_file(const char * path){
		open_device(path);

		if (!is_dev)
			return;

		init_v4l2_device();
	}

	void start_capturing(){
		enum v4l2_buf_type type;

		if (!is_dev)
			return;

		if (io_method == IO_METHOD_READ)
			return;

		for (int i = 0; i < BUF_COUNT; ++i) {
            struct v4l2_buffer buf;

            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                errno_exit("VIDIOC_QBUF");
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
            errno_exit("VIDIOC_STREAMON");
	}

    bool read_frame_file(void * dest){
	    if(is_dev == false)
	    	return read_raw(dest);

    	do {
			fd_set fds;
		    struct timeval tv;
		    int r;

		    FD_ZERO(&fds);
		    FD_SET(fd, &fds);

		    r = select(fd + 1, &fds, NULL, NULL, &tv);

			if (-1 == r) {
				    if (EINTR == errno)
				            continue;
				    errno_exit("select");
			}

			if (0 == r) {
				    fprintf(stderr, "select timeout\n");
				    exit(EXIT_FAILURE);
			}
		} while (!read_v4l2_frame(dest));
		return true;
    }
};

#endif // __DEV_HANDLER_HPP__
