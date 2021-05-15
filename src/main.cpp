#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include <getopt.h>     /* getopt_long() */

#include "memory_mlx90640.hpp"
#include "mlx90640.hpp"
#include "dev_handler.hpp"

static const char short_options[] = "d:n:hmrf:S:R:CX";

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
    { "extended-format",  no_argument,  NULL, 'X' },
    { 0, 0, 0, 0 }
};

static void usage(FILE *fp, int argc, char**argv)
{
    fprintf(fp,
            "Usage: %s [options]\n\n"
            "Options:\n"
            "-d | --device PATH         [REQUIRED] Video device file path\n"
            "           If the file appear to be not a device file,\n"
            "           then the program will fall back to raw file read.\n"
            "-n | --nvram PATH          [REQUIRED] NVRAM file path\n"
            "-h | --help                Print this message\n"
            "-m | --mmap                Use memory mapped buffers [default]\n"
            "-r | --read                Use read() calls\n"
            "-f | --fps                 Set feed update frequency [default: 4FPS]\n"
            "               If device is raw file and fps is not given or -1,\n"
            "               then the program will process the file as fast as possible.\n"
            "-S | --save PATH           (Debug) Path to save artificially contrasted gray16-le raw feed\n"
            "               Output = (Tobj - 20) * 3000\n"
            "-R | --save-raw PATH       (Debug) Path to save raw frame data feed\n"
            "-C | --ignore-EE-check     Skip NVRAM validity check\n"
            "-X | --extended-format     (Raw file read only) Treat the file as 27 lines per frame\n"
            "",
            argv[0]);
}

int main(int argc, char **argv) {
	mlx90640 mlx = mlx90640();
	dev_handler* device;

	char * dev_name = NULL;
	char * nv_name = NULL;

	char * fps_ = NULL;
	int fps = -1;

	bool save = false;
	char * save_path = NULL;
	bool save_raw = false;
	char * save_raw_path = NULL;

	int io_method = dev_handler::IO_METHOD_MMAP;
	bool ignore_ee_check = false;
	bool extended_format = false;

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

	    case 'h':
	        usage(stdout, argc, argv);
	        exit(EXIT_SUCCESS);
	        break;

	    case 'm':
	        io_method = dev_handler::IO_METHOD_MMAP;
	        break;

	    case 'r':
	        io_method = dev_handler::IO_METHOD_READ;
	        break;

	    case 'f':
	        fps_ = optarg;
	        fps = (int)std::stof(fps_);
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

	    case 'X':
	        extended_format = true;
	        break;

	    default:
	        fprintf(stderr, "Unrecognized option: %c\n", c);
	        usage(stdout, argc, argv);
	        exit(EXIT_FAILURE);
	        break;
	    }
	}

	if(dev_name == NULL || nv_name == NULL){
	    printf("Required option not given\n");
        usage(stdout, argc, argv);
	    exit(EXIT_FAILURE);
	}

    device = new dev_handler(io_method, fps, extended_format);
    device->init_frame_file(dev_name);

	if(!mlx.init_ee(nv_name, ignore_ee_check)){
	    printf("NVMEM initialization error\n");
	    exit(EXIT_FAILURE);
	}

	mlx.init_frame_file(device);

	uint16_t To_int[0x300];
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
			    }
		    }
		    if(save)
		        fwrite(To_int, sizeof(uint16_t), 0x300, save_LE16_frm);
		    if(save_raw)
		        fwrite(mlx.Pix_Raw_(), sizeof(uint16_t), device->is_extended() ? 0x360 : 0x340, save_pixel_raw);
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
