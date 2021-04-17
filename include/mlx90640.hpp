#ifndef __MLX90640_H__
#define __MLX90640_H__

#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include "memory_mlx90640.hpp"

class mlx90640 {
public:
	mlx90640() { ram_fd = -1; };
	~mlx90640() {};

private:
	mlx90640_nvmem_ ee;

	mlx90640_ram_ ram;
	ssize_t ram_fd;

private:
	int K_Vdd_EE;
	int Vdd_25_EE;
	double a_PTAT;
	double K_V_PTAT;
	double K_T_PTAT;
	int V_PTAT_25;

public: // temporary for debug
	int get_K_Vdd_EE() {return K_Vdd_EE;}
	int get_Vdd25_EE() {return Vdd_25_EE;}
	double get_a_PTAT() {return a_PTAT;}
	double get_K_V_PTAT() {return K_V_PTAT;}
	double get_K_T_PTAT() {return K_T_PTAT;}
	int get_V_PTAT_25() {return V_PTAT_25;}

    void print_ee(void) {
    	std::printf("size of ee.word: %d\n", (int)sizeof(ee.word_));
    	std::printf("size of ee.named: %d\n", (int)sizeof(ee.named));
    	std::printf("address of ee.word: %u\n", (unsigned)ee.word_);
    	std::printf("address of ee.named: %u\n", (unsigned)&(ee.named));
    	std::printf("%u %u %u %u %u\n",
    		(unsigned)ee.named.ee_conf,
    		(unsigned)&(ee.named.ee_2410),
    		(unsigned)&(ee.named.ee_2420),
    		(unsigned)&(ee.named.ee_GAIN),
    		(unsigned)ee.named.ee_PIX);

        for(int i=0; i<0x340; i++)
	    {
	        char buffer[5];
	        std::snprintf(buffer, 5, "%04hX", ee.word_[i]);
	        std::cout << buffer;
	        if(i % 16 == 15)
	            std::cout << "\n";
	        else
	            std::cout << " ";
	    }
    }

private:
	bool read_ee(const char * path) {
		int fd = open(path, O_RDONLY);
        if(fd == -1)
            return false;
        ssize_t rdsz_ = read(fd, (unsigned char *)(ee.word_),
	        sizeof(ee) / sizeof(char));
    	std::cout << rdsz_ << " bytes read\n";
    	close(fd);
    	return true;
    }

    unsigned short fetch_EE_address(int address){
		const int OFFSET = 0x2400;

        if(address < OFFSET || address > OFFSET + 0x33F){
        //if(address < OFFSET || address >= OFFSET + 0x340){
            printf("bad EE addr, %d.\n", address);
            return 0;
        }
        return le16toh(ee.word_[address - OFFSET]);
    }

public:
    bool init_ee(const char * path) {
		bool rtn = read_ee(path);
		if(rtn == false)
			return false;

        union {
            uint16_t word_;
            struct bitfield_ee2433 {
                unsigned int Vdd_25_EE : 8;
                int K_Vdd_EE : 8;
            } bf;
        } ee2433;
        ee2433.word_ = fetch_EE_address(0x2433);
        Vdd_25_EE = ee2433.bf.Vdd_25_EE;
        K_Vdd_EE = ee2433.bf.K_Vdd_EE;

        union {
            uint16_t word_;
            struct bitfield_ee2410 {
                unsigned int scale_Occ_rem: 4;
                unsigned int scale_Occ_col: 4;
                unsigned int scale_Occ_row: 4;
                unsigned int a_PTAT_EE: 4;
            } bf;
        } ee2410;
        ee2410.word_ = fetch_EE_address(0x2410);
        a_PTAT = (double)ee2410.bf.a_PTAT_EE / 4.0 + 8.0;

        union {
            uint16_t word_;
            struct bitfield_ee2432 {
                unsigned int K_T_PTAT_l8: 8;
                unsigned int K_T_PTAT_u2: 2;
                int K_V_PTAT_EE: 6;
            } bf;
        } ee2432;
        ee2432.word_ = fetch_EE_address(0x2432);
        K_V_PTAT = (double)ee2432.bf.K_V_PTAT_EE / (double)(1 << 12);
        int K_T_PTAT_EE = (ee2432.bf.K_T_PTAT_u2 << 8)
                            | (ee2432.bf.K_T_PTAT_l8);
        K_T_PTAT = (double)K_T_PTAT_EE / 8.0;

        V_PTAT_25 = ee.named.PTAT_25;

        return true;
    }
};

class regmap{
public:
    regmap() {
        fd_ = -1;
    }
    ~regmap() {}

private:
    mlx90640_ram_ regmap_;
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
        rdsz_ = read(fd_, (unsigned char *)(regmap_.word_),
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
	        std::snprintf(buffer, 5, "%04hX", regmap_.word_[i]);
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
        return le16toh(regmap_.word_[address - OFFSET]);
    }

    short get_dV_raw(){
        return regmap_.named_.VDD_raw;
    }

    short get_V_PTAT(){
        return regmap_.named_.Ta_PTAT; // p18 says Ta_PTAT but p23 says V_PTAT
    }

    short get_V_BE(){
        return regmap_.named_.V_BE;
    }
};

#endif // __MLX90640_H__
