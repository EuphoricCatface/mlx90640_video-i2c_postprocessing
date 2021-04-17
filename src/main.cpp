#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include "memory_mlx90640.hpp"
#include "mlx90640.hpp"



int main() {
	std::cout << "Hello World\n";

	mlx90640 device = mlx90640();
	device.init_ee("/home/USER/mlx/nvmem");
	device.init_frame_file("/home/USER/mlx/mlx_v4l2_raw_frames");

	//device.init_ee("/sys/bus/i2c/devices/1-0033/nvmem/nvram");
	// printf("EE[2433] is %04hX\n", nv.fetch_EE_address(0x2433));
	unsigned int Vdd_25_EE = device.get_Vdd25_EE();
	int K_Vdd_EE = device.get_K_Vdd_EE();
	printf("K_Vdd_EE is %hhd, Vdd_25_EE is %hhu\n", K_Vdd_EE, Vdd_25_EE);
	double a_PTAT = device.get_a_PTAT();
	printf("a_PTAT is %lf\n", a_PTAT);
	double K_V_PTAT = device.get_K_V_PTAT();
	double K_T_PTAT = device.get_K_T_PTAT();
	int V_PTAT_25 = device.get_V_PTAT_25();
	// printf("EE[2432] is %04hX\n", nv.fetch_EE_address(0x2432));
	printf("K_V_PTAT is %lf, K_T_PTAT is %lf, V_PTAT_25 is %hd\n", K_V_PTAT, K_T_PTAT, V_PTAT_25);

	while(1){
	    if(!device.process_frame_file())
	        break;

	    device.process_data();
	}

    printf("closing\n");

	return 0;
}
