#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include "memory_mlx90640.hpp"
#include "mlx90640.hpp"

int main() {
	mlx90640 device = mlx90640();
	device.init_ee("/home/USER/mlx/nvmem");
	device.init_frame_file("/home/USER/mlx/mlx_v4l2_raw_frames");

	//device.init_ee("/sys/bus/i2c/devices/1-0033/nvmem/nvram");

	uint16_t To_int[0x300];
	FILE* save_raw;
	save_raw = fopen("/home/USER/raw", "wb");

	while(1){
	    if(!device.process_frame_file())
	        break;

	    device.process_frame();
	    device.process_pixel();

		for(int row = 0; row < 24; row++){
			for(int col = 0; col < 32; col++){
				To_int[row * 32 + col] = ((device.To_())[row * 32 + col] - 20) * 3000;
			}
		}
		fwrite(To_int, sizeof(uint16_t), 0x300, save_raw);

	}

    printf("closing\n");
    fclose(save_raw);

	return 0;
}
