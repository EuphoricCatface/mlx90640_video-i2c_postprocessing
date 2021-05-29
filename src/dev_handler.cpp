#include "dev_handler.hpp"

void dev_handler::open_device(const char * path) {
    struct stat st;
    struct v4l2_format fmt;

    if (-1 == stat(path, &st)) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                 path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "Warning: %s is not a device.\n", path);
        fprintf(stderr, "Warning: Falling back to raw file parsing.\n");
        is_dev = false;
    }

    if (is_dev)
        fd = open(path, O_RDWR /* required */ | O_NONBLOCK, 0);
    else{
        fd = open(path, O_RDONLY);
        open_ = true;
        return;
    }

    if (-1 == fd) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
                path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
        errno_exit("VIDIOC_G_FMT");

    printf("Video width: %d, height: %d\n",
            fmt.fmt.pix.width,
            fmt.fmt.pix.height);
    switch (fmt.fmt.pix.height) {
        case 27:
            extended = true;
            break;
        case 26:
            extended = false;
            fprintf(stderr, "Warning: video feed doesn't appear to provide internal register information\n");
            fprintf(stderr, " - No proper \"subpage\" compensation available!\n");
            break;
        default:
            fprintf(stderr, "Wrong video height: %d\n", fmt.fmt.pix.height);
            exit(EXIT_FAILURE);
    }

    switch (io_method) {
    case IO_METHOD_READ:
            init_read(fmt.fmt.pix.sizeimage);
            break;

    case IO_METHOD_MMAP:
            init_mmap();
            break;
    }
    open_ = true;
}

void dev_handler::init_v4l2_device(void) {
    struct v4l2_capability cap;
    struct v4l2_streamparm parm;
    struct v4l2_fract fract(1, 4);

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        if (EINVAL == errno) {
            fprintf(stderr, "Device is not a V4L2 device\n");
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }

    CLEAR(parm);
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_G_PARM, &parm)) {
        errno_exit("VIDIOC_G_PARM");
    }

    switch (fps) {
        case 0: // yeah I'm lazy :P
            fract.numerator=2, fract.denominator=1;
            break;
        case 1:
            fract.numerator=1, fract.denominator=1;
            break;
        case 2:
            fract.numerator=1, fract.denominator=2;
            break;
        case 8:
            fract.numerator=1, fract.denominator=8;
            break;
        case 16:
            fract.numerator=1, fract.denominator=16;
            break;
        case 32:
            fract.numerator=1, fract.denominator=32;
            break;
        case 64:
            fract.numerator=1, fract.denominator=64;
            break;
        default:
            fprintf(stderr, "Warning: FPS unrecognized, defaulting to 4Hz\n");
            // fall through
        case 4:
            fract.numerator=1, fract.denominator=4;
            break;
    }
    parm.parm.capture.timeperframe = fract;


    if (-1 == xioctl(fd, VIDIOC_S_PARM, &parm)) {
        errno_exit("VIDIOC_S_PARM");
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
    init = true;
}


void dev_handler::init_mmap(void) {
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


void dev_handler::init_read(unsigned int buffer_size) {
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

bool dev_handler::read_raw(void * dest) {
    int size = extended ? 0x6c0 : 0x680;
    int rdsz_ = read(fd, (unsigned char *)(dest),
        //sizeof(dest) / sizeof(char)
        size);

    switch (fps) {
        case -1:
            break;
        case 0:
            usleep(2000000);
            break;
        default:
            if (fps > 0)
                usleep(1000000 / fps);
            else {
                fprintf(stderr, "FPS is negative\n");
                exit(EXIT_FAILURE);
            }
    }

    if(rdsz_ < size) {
        std::cout << "A frame did not reach its full size.\n";
        return false;
    }
    return true;
}

int dev_handler::read_v4l2_frame(void * dest) {
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

void dev_handler::stop_capturing(void) {
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

void dev_handler::uninit_device(void) {
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

void dev_handler::close_device(void) {
    if (-1 == close(fd))
        errno_exit("close");

    fd = -1;
}

void dev_handler::init_frame_file(const char * path) {
    open_device(path);

    if (!is_dev)
        return;

    init_v4l2_device();
}

void dev_handler::start_capturing(void) {
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
    capturing = true;
}

bool dev_handler::read_frame_file(void * dest) {
    if(is_dev == false)
        return read_raw(dest);

    do {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

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

