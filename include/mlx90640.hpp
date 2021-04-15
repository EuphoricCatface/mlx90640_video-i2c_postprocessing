#ifndef __MLX90640_H__
#define __MLX90640_H__

#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include "memory_mlx90640.hpp"

class nvram{
public:
    nvram() {}
    ~nvram() {}
private:
    unsigned short nvram_[0x400];
    ssize_t rdsz_;

    const int OFFSET = 0x2400;

public:
    bool read_mem(const char * path) {
        int fd = open(path, O_RDONLY);
        if(fd == -1)
            return false;
        rdsz_ = read(fd, (unsigned char *)nvram_,
	        sizeof(nvram_) / sizeof(char));
    	std::cout << rdsz_ << " bytes read\n";
    	close(fd);
    	return true;
    }

    void print(void) {
        for(int i=0; i<rdsz_; i++)
	    {
	        char buffer[5];
	        std::snprintf(buffer, 5, "%04hX", nvram_[i]);
	        std::cout << buffer;
	        if(i % 16 == 15)
	            std::cout << "\n";
	        else
	            std::cout << " ";
	    }
    }

    unsigned short fetch_EE_address(int address){
        if(address < 0x2400 || address > 0x273F){
            printf("bad EE addr, %d.\n", address);
            return 0;
        }
        return le16toh(nvram_[address - OFFSET]);
    }

    int8_t get_K_Vdd_EE(){
        union {
            uint16_t word_;
            struct bitfield_ee2433 {
                uint8_t Vdd_25_EE : 8;
                int8_t K_Vdd_EE : 8;
            } bf;
        } ee2433;
        ee2433.word_ = fetch_EE_address(0x2433);
        return ee2433.bf.K_Vdd_EE;
    }

    uint8_t get_Vdd_25_EE(){
        union {
            uint16_t word_;
            struct bitfield_ee2433 {
                uint8_t Vdd_25_EE : 8;
                int8_t K_Vdd_EE : 8;
            } bf;
            // uint8_t arr[2];
        } ee2433;
        ee2433.word_ = fetch_EE_address(0x2433);

	    // printf("bf is %d, arr is %d\n", ee2433.bf.Vdd_25_EE, ee2433.arr[0]);
	    // confirming bitfield works. Tested on ARM
        return ee2433.bf.Vdd_25_EE;
    }

    double get_a_PTAT(){
        union {
            uint16_t word_;
            struct bitfield_ee2410 {
                uint8_t scale_Occ_rem: 4;
                uint8_t scale_Occ_col: 4;
                uint8_t scale_Occ_row: 4;
                uint8_t a_PTAT_EE: 4;
            } bf;
        } ee2410;
        ee2410.word_ = fetch_EE_address(0x2410);
        return (double)ee2410.bf.a_PTAT_EE / 4.0 + 8.0;
    }

    double get_K_V_PTAT(){
        union {
            uint16_t word_;
            struct bitfield_ee2432 {
                uint8_t K_T_PTAT_l8: 8;
                uint8_t K_T_PTAT_u2: 2;
                int8_t K_V_PTAT_EE: 6;
            } bf;
        } ee2432;
        ee2432.word_ = fetch_EE_address(0x2432);
        return (double)ee2432.bf.K_V_PTAT_EE / (double)(1 << 12);
    }

    double get_K_T_PTAT(){
        union {
            uint16_t word_;
            struct bitfield_ee2432 {
                uint8_t K_T_PTAT_l8: 8;
                uint8_t K_T_PTAT_u2: 2;
                int8_t K_V_PTAT_EE: 6;
            } bf;
        } ee2432;
        ee2432.word_ = fetch_EE_address(0x2432);
        int K_T_PTAT_EE = (ee2432.bf.K_T_PTAT_u2 << 8)
                            | (ee2432.bf.K_T_PTAT_l8);
        return (double)K_T_PTAT_EE / 8.0;
    }

    int16_t get_V_PTAT_25(){
        union {
            uint16_t word_;
            int16_t V_PTAT_25;
        } ee2431;
        ee2431.word_ = fetch_EE_address(0x2431);
        return ee2431.V_PTAT_25;
    }

};

class regmap{
public:
    regmap() {
        fd_ = -1;
    }
    ~regmap() {}

private:
    unsigned short regmap_[0x340];
    ssize_t rdsz_;

    const int OFFSET = 0x400;
    int fd_;
public:
    bool open_path(const char * path){
        fd_ = open(path, O_RDONLY);
        if(fd_ == -1)
            return false;
        return true;
    }

    void close_path() {
        close(fd_);
    }

    bool read_mem(){
        rdsz_ = read(fd_, (unsigned char *)regmap_,
	        sizeof(regmap_) / sizeof(char));
    	if(rdsz_ != 0x680) {
    	    std::cout << "A frame did not reach its full size.\n";
    	    return false;
    	}
    	return true;
    }

    void print(void) {
        for(int i=0; i<rdsz_; i++)
	    {
	        char buffer[5];
	        std::snprintf(buffer, 5, "%04hX", regmap_[i]);
	        std::cout << buffer;
	        if(i % 16 == 15)
	            std::cout << "\n";
	        else
	            std::cout << " ";
	    }
    }

    unsigned short fetch_RAM_address(int address){
        if(address < 0x400 || address > 0x73F){
            printf("bad RAM addr, %d\n", address);
            return 0;
        }
        return le16toh(regmap_[address - OFFSET]);
    }

    short get_dV_raw(){
        union {
            uint16_t word_;
            int16_t dV_raw;
        } ram072A;

        ram072A.word_ = fetch_RAM_address(0x072A);
        return ram072A.dV_raw;
    }

    short get_V_PTAT(){
        union {
            uint16_t word_;
            int16_t V_PTAT;
        } ram0720;

        ram0720.word_ = fetch_RAM_address(0x0720);
        return ram0720.V_PTAT;
    }

    short get_V_BE(){
        union {
            uint16_t word_;
            int16_t V_BE;
        } ram0700;

        ram0700.word_ = fetch_RAM_address(0x0700);
        return ram0700.V_BE;
    }
};

#endif // __MLX90640_H__
