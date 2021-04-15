#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include "memory_mlx90640.hpp"
#include "mlx90640.hpp"



int main() {
	std::cout << "Hello World\n";

	nvram nv = nvram();
	nv.read_mem("/home/USER/mlx/nvmem");
	nv.print();

	printf("EE[2433] is %04hX\n", nv.fetch_EE_address(0x2433));
	uint8_t Vdd_25_EE = nv.get_Vdd_25_EE();
	int8_t K_Vdd_EE = nv.get_K_Vdd_EE();
	printf("K_Vdd_EE is %hhd, Vdd_25_EE is %hhu\n", K_Vdd_EE, Vdd_25_EE);
	double a_PTAT = nv.get_a_PTAT();
	printf("a_PTAT is %lf\n", a_PTAT);
	double K_V_PTAT = nv.get_K_V_PTAT();
	double K_T_PTAT = nv.get_K_T_PTAT();
	int16_t V_PTAT_25 = nv.get_V_PTAT_25();
	printf("EE[2432] is %04hX\n", nv.fetch_EE_address(0x2432));
	printf("K_V_PTAT is %lf, K_T_PTAT is %lf, V_PTAT_25 is %hd\n", K_V_PTAT, K_T_PTAT, V_PTAT_25);

	regmap ram = regmap();
	ram.open_path("/home/USER/mlx/mlx_v4l2_raw_frames");

	while(1){
	    if(!ram.read_mem())
	        break;

	    int16_t dV_raw = ram.get_dV_raw();
	    printf("dV_raw is %04hX, ", dV_raw);
	    double dV = (double)(-((int)Vdd_25_EE << 5) + dV_raw + 16384) / (double) K_Vdd_EE / 32.0;
	    printf("dV: %lf\n", dV);

	    int16_t V_BE = ram.get_V_BE();
	    int16_t V_PTAT = ram.get_V_PTAT();

	    double V_PTAT_art = (double)(1 << 18) / (a_PTAT + (double)V_BE / (double)V_PTAT);
	    printf("V_BE, V_PTAT, V_PTAT_art: %hd, %hd, %lf\n", V_BE, V_PTAT, V_PTAT_art);

	    double dTa = (V_PTAT_art / (1.0 + K_V_PTAT * dV) - V_PTAT_25) / K_T_PTAT;
	    printf("dTa: %lf\n", dTa);
	}

    printf("closing\n");
	ram.close_path();

	return 0;
}
