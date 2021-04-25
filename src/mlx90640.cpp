#include <cstring>

#include "mlx90640.hpp"

mlx90640 * adapter_ptr;

void drv_adapter_init(mlx90640 * ptr){
	adapter_ptr = ptr;
}

int MLX90640_I2CRead(uint8_t slaveAddr,uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data){
    static int data_avail = 0;
    static int subpg = 0;
	switch(startAddress){
	case 0x400:
		memcpy(data, adapter_ptr->ram.word_, nMemAddressRead * 2);
		subpg++;
		break;
	case 0x2400:
		memcpy(data, adapter_ptr->ee.word_, nMemAddressRead * 2);
		break;
	case 0x8000:
		*data = (data_avail%2 ? 0x0008 : 0) | // Pretend data is <s>always</s> available
		                // actually, main logic cannot proceed if this is stuck
				(subpg%2 ? 0x0001 : 0); // <s>Subpage does not matter if TGC is 0</s>
				        // actually, the main logic tries to calculate only corresponding subpage
		data_avail++;
		break;
	case 0x800D:
		*data = 0x1000 // Chess pattern
				| 0x0800; // 18bit ADC resolution
		// The rest are all get & set pair
		break;
	default:
		break;
	}
	return  0;
}

int MLX90640_I2CWrite(uint8_t slaveAddr,uint16_t writeAddress, uint16_t data) {}
