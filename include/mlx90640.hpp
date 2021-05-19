#ifndef __MLX90640_H__
#define __MLX90640_H__

#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <cmath>

#include "memory_mlx90640.hpp"
#include "dev_handler.hpp"

class mlx90640 {
public:
    mlx90640() { dev = nullptr; }
    ~mlx90640() {}

private:
    mlx90640_nvmem_ ee;

    int K_Vdd_EE;
    int Vdd_25_EE;
    double a_PTAT;
    double K_V_PTAT;
    double K_T_PTAT;
    int V_PTAT_25;

    int offset_ref[0x300];
    double a_ref[0x300];

    int gain_ee;
    double K_V[2][2];
    double K_Ta[0x300];

public: // temporary for debug
    int get_K_Vdd_EE() {return K_Vdd_EE;}
    int get_Vdd25_EE() {return Vdd_25_EE;}
    double get_a_PTAT() {return a_PTAT;}
    double get_K_V_PTAT() {return K_V_PTAT;}
    double get_K_T_PTAT() {return K_T_PTAT;}
    int get_V_PTAT_25() {return V_PTAT_25;}

    void print_ee(void);

private:
    bool read_ee(const char * path);
    unsigned short fetch_EE_address(int address);

public:
    bool init_ee(const char * path, bool ignore_ee_check);

private:
    mlx90640_ram_ ram;
    dev_handler * dev;

    int VDD_raw;
    int V_PTAT;
    int V_BE;
    int gain_ram;

    bool extended;
    int subpage;

public: // temporary for debug
    int get_VDD_raw() {return VDD_raw;}
    int get_V_PTAT() {return V_PTAT;}
    int get_V_BE() {return V_BE;}

private:
    unsigned short fetch_RAM_address(int address);
    unsigned short fetch_reg_address(int address);

public:
    void init_frame_file(dev_handler* dev_) {
        dev = dev_;

        extended = dev->is_extended();
        dev->start_capturing();
    }

    bool process_frame_file() {
        if (!dev->read_frame_file(ram.word_))
            return false;

        VDD_raw = ram.named.VDD_raw;
        V_PTAT = ram.named.Ta_PTAT; // p18 says Ta_PTAT but p23 says V_PTAT
        V_BE = ram.named.V_BE;

        gain_ram = ram.named.ram_GAIN;

        return true;
    }

    enum PIX_NOTE {
        MIN_T,
        MAX_T,
        SCENE_CENTER
    };

    struct pixel {
        int x;
        int y;
        double T;
    };

private:
    double dV;
    double V_PTAT_art;
    double dTa;
    double gain;
    double T_ar;
    double e;

    double pix[0x300];
    double To[0x300];

    pixel pix_list[3];

public:
    void process_frame(void);
    void process_pixel(void);

    const double * To_() { return To; }
    const uint16_t * Pix_Raw_() { return ram.word_; }
    const pixel * pix_notable() { return pix_list; }

};

#endif // __MLX90640_H__
