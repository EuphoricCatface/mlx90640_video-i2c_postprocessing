#include <iostream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include "memory_mlx90640.hpp"
#include "mlx90640.hpp"
#include "pimoroni_driver/MLX90640_API.hpp"

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

	drv_adapter_init(&device);
	// MELEXIS
    static uint16_t eeMLX90640[832];
    float emissivity = 1;
    uint16_t frame[834];
    float eTa;

    paramsMLX90640 p_mlx90640;
    MLX90640_DumpEE(-1, eeMLX90640);
    MLX90640_ExtractParameters(eeMLX90640, &p_mlx90640);

    static float mlx90640To[768];
    // MELEXIS

	uint16_t To_int[0x300];
	FILE* save_raw;
	save_raw = fopen("/home/USER/raw", "wb");

	while(1){
	    if(!device.process_frame_file())
	        break;

	    //device.process_frame();
	    //device.process_pixel();

	    //MELEXIS
        MLX90640_GetFrameData(-1, frame);
        eTa = MLX90640_GetTa(frame, &p_mlx90640);
        MLX90640_CalculateTo(frame, &p_mlx90640, emissivity, eTa, mlx90640To);
        //MELEXIS


		for(int row = 0; row < 24; row++){
			for(int col = 0; col < 32; col++){
			    if(mlx90640To[row * 32 + col] < 0.01f) continue;
				To_int[row * 32 + col] = (mlx90640To[row * 32 + col] - 20) * 3000;
			}
		}
		fwrite(To_int, sizeof(uint16_t), 0x300, save_raw);

	}

    printf("closing\n");
    fclose(save_raw);

	return 0;
}
