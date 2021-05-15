#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include <getopt.h>     /* getopt_long() */

#include "memory_mlx90640.hpp"
#include "mlx90640.hpp"
#include "dev_handler.hpp"

static const char short_options[] = "d:n:hmrf:S:R:C";

static const struct option
long_options[] = {
    { "device",     required_argument,  NULL, 'd' },
    { "nvram",      required_argument,  NULL, 'n' },
    { "help",       no_argument,        NULL, 'h' },
    { "mmap",       no_argument,        NULL, 'm' },
    { "read",       no_argument,        NULL, 'r' },
    { "fps",        required_argument,  NULL, 'f' },
    { "save",       required_argument,  NULL, 'S' },
    { "save-raw",   required_argument,  NULL, 'R' },
    { "ignore-EE-check",  no_argument,  NULL, 'C' },
    { 0, 0, 0, 0 }
};

int main(int argc, char **argv) {
	mlx90640 mlx = mlx90640();
	dev_handler* device;

	char * dev_name = NULL;
	char * nv_name = NULL;

	char * fps_ = NULL;
	int fps;

	bool save = false;
	char * save_path = NULL;
	bool save_raw = false;
	char * save_raw_path = NULL;

	int io_method = dev_handler::IO_METHOD_MMAP;
	bool ignore_ee_check = false;

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

	    case 'S':
	        save = true;
	        save_path = optarg;
	        break;

	    case 'R':
	        save_raw = true;
	        save_raw_path = optarg;
	        break;

	    case 'C':
	        ignore_ee_check = true;
	        break;

	    default:
	        break;
	    }
	}

	if(dev_name == NULL || nv_name == NULL){
	    printf("Required option not given");
	    exit(EXIT_FAILURE);
	}

    device = new dev_handler(io_method, fps);
    device->init_frame_file(dev_name);

	if(!mlx.init_ee(nv_name, ignore_ee_check)){
	    printf("NVMEM initialization error\n");
	    exit(EXIT_FAILURE);
	}

	mlx.init_frame_file(device);

	uint16_t To_int[0x300];
	uint16_t pixel_raw[0x300];
	FILE* save_LE16_frm;
	FILE* save_pixel_raw;
	if(save)
	    save_LE16_frm = fopen(save_path, "wb");
	if(save_raw)
	    save_pixel_raw = fopen(save_raw_path, "wb");

	while(1){
	    if(!mlx.process_frame_file())
	        break;

	    mlx.process_frame();
	    mlx.process_pixel();

        if(save || save_raw){
		    for(int row = 0; row < 24; row++){
			    for(int col = 0; col < 32; col++){
				    To_int[row * 32 + col] = ((mlx.To_())[row * 32 + col] - 20) * 3000;
				    pixel_raw[row * 32 + col] = (mlx.Pix_Raw_())[row * 32 + col];
			    }
		    }
		    if(save)
		        fwrite(To_int, sizeof(uint16_t), 0x300, save_LE16_frm);
		    if(save_raw)
		        fwrite(pixel_raw, sizeof(uint16_t), 0x300, save_pixel_raw);
		}

	}

    printf("closing\n");
    if(save){
        fclose(save_LE16_frm);
    }
    if(save_raw){
        fclose(save_pixel_raw);
    }

    delete device;
	return 0;
}
