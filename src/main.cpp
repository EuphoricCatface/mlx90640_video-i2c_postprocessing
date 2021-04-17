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
	//device.init_ee("/sys/bus/i2c/devices/1-0033/nvmem/nvram");
	device.init_ee("/home/USER/mlx/nvmem");
	device.print_ee();

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

	device.init_frame_file("/home/USER/mlx/filesinkdump");

	while(1){
	    if(!device.process_frame_file())
	        break;

	    int16_t dV_raw = device.get_VDD_raw();
	    printf("dV_raw is %04hX, ", dV_raw);
	    double dV = (double)(-((int)Vdd_25_EE << 5) + dV_raw + 16384) / (double) K_Vdd_EE / 32.0;
	    printf("dV: %lf\n", dV);

	    int16_t V_BE = device.get_V_BE();
	    int16_t V_PTAT = device.get_V_PTAT();

	    double V_PTAT_art = (double)(1 << 18) / (a_PTAT + (double)V_BE / (double)V_PTAT);
	    printf("V_BE, V_PTAT, V_PTAT_art: %hd, %hd, %lf\n", V_BE, V_PTAT, V_PTAT_art);

	    double dTa = (V_PTAT_art / (1.0 + K_V_PTAT * dV) - V_PTAT_25) / K_T_PTAT;
	    printf("dTa: %lf\n", dTa);
	}

    printf("closing\n");

	return 0;
}
