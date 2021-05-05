#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include <getopt.h>     /* getopt_long() */

#include "memory_mlx90640.hpp"
#include "mlx90640.hpp"
#include "dev_handler.hpp"

static const char short_options[] = "d:n:hmrf:s:";

static const struct option
long_options[] = {
    { "device", required_argument,  NULL, 'd' },
    { "nvram",  required_argument,  NULL, 'n' },
    { "help",   no_argument,        NULL, 'h' },
    { "mmap",   no_argument,        NULL, 'm' },
    { "read",   no_argument,        NULL, 'r' },
    { "fps",    required_argument,  NULL, 'f' },
    { "save",   required_argument,  NULL, 's' },
    { 0, 0, 0, 0 }
};

int main(int argc, char **argv) {
	mlx90640 device = mlx90640();

	char * dev_name = NULL;
	char * nv_name = NULL;

	char * fps_ = NULL;
	int fps;

	bool save = false;
	char * save_path = NULL;

	int io_method = dev_handler::IO_METHOD_MMAP;

	for (;;){
	    int idx;
	    int c;

	    c = getopt_long(argc, argv,
	                    short_options, long_options, &idx);

	    if (c == -1)
	        break;

	    switch (c) {
	    case 0: /* getopt_long() flag */
	        break;

	    case 'd':
	        dev_name = optarg;
	        break;

	    case 'n':
	        nv_name = optarg;
	        break;

	    case 'm':
	        io_method = dev_handler::IO_METHOD_MMAP;
	        break;

	    case 'r':
	        io_method = dev_handler::IO_METHOD_READ;
	        break;

	    case 'f':
	        printf("fps option nyi");
	        fps_ = optarg;
	        fps = -1;
	        break;

	    case 's':
	        save = true;
	        save_path = optarg;
	        break;

	    default:
	        break;
	    }
	}

	if(dev_name == NULL || nv_name == NULL){
	    printf("Required option not given");
	    exit(EXIT_FAILURE);
	}

	device.init_ee(nv_name);
	device.init_frame_file(dev_name, io_method, fps);

	uint16_t To_int[0x300];
	FILE* save_raw;
	if(save)
	    save_raw = fopen(save_path, "wb");

	while(1){
	    if(!device.process_frame_file())
	        break;

	    device.process_frame();
	    device.process_pixel();

        if(save){
		    for(int row = 0; row < 24; row++){
			    for(int col = 0; col < 32; col++){
				    To_int[row * 32 + col] = ((device.To_())[row * 32 + col] - 20) * 3000;
			    }
		    }
		    fwrite(To_int, sizeof(uint16_t), 0x300, save_raw);
		}

	}

    printf("closing\n");
    if(save){
        fclose(save_raw);
    }
	return 0;
}
