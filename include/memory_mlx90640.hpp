#include <cstdint>

#ifndef __MEMORY_MLX90640_H__
#define __MEMORY_MLX90640_H__

union OCC_ROW_ {
    uint16_t word_;
    struct {
        int OCC_ROW_1_ : 4;
        int OCC_ROW_2_ : 4;
        int OCC_ROW_3_ : 4;
        int OCC_ROW_4_ : 4;
    } bf;
};

union OCC_COL_ {
    uint16_t word_;
    struct {
        int OCC_COL_1_ : 4;
        int OCC_COL_2_ : 4;
        int OCC_COL_3_ : 4;
        int OCC_COL_4_ : 4;
    } bf;
};

union ACC_ROW_ {
    uint16_t word_;
    struct {
        int ACC_ROW_1_ : 4;
        int ACC_ROW_2_ : 4;
        int ACC_ROW_3_ : 4;
        int ACC_ROW_4_ : 4;
    } bf;
};

union ACC_COL_ {
    uint16_t word_;
    struct {
        int ACC_COL_1_ : 4;
        int ACC_COL_2_ : 4;
        int ACC_COL_3_ : 4;
        int ACC_COL_4_ : 4;
    } bf;
};

union ee_PIX_ {
    uint16_t word_;
    struct {
        bool outlier            : 1;
        int K_Ta                : 3;
        unsigned int PIX_a_l4   : 4;
        unsigned int PIX_a_h2   : 2;
        int PIX_OFF             : 6;
    } bf;
};

union mlx90640_nvmem_ {
    uint16_t word_[0x340];
    struct {
        uint16_t ee_conf [0x10]; // 0x2400

        uint16_t ee_2410;
        int16_t PIX_OS_AVG;
        OCC_ROW_ OCC_ROW [6];
        OCC_COL_ OCC_COL [8];

        uint16_t ee_2420;
        uint16_t PIX_SENS_AVG;
        ACC_ROW_ ACC_ROW [6];
        ACC_COL_ ACC_COL [8];

        int16_t ee_GAIN; // 0x2430
        int16_t PTAT_25;
        uint16_t ee_2432;
        uint16_t ee_2433;
        uint16_t ee_2434;
        uint16_t ee_2435;
        uint16_t ee_2436;
        uint16_t ee_2437;
        uint16_t ee_2438;
        uint16_t ee_2439;
        uint16_t ee_243a;
        uint16_t ee_243b;
        uint16_t ee_243c;
        uint16_t ee_243d;
        uint16_t ee_243e;
        uint16_t ee_243f;

        ee_PIX_ ee_PIX [0x300];
    } named;
};

union mlx90640_ram_ {
    uint16_t word_[0x340];
    struct {
        uint16_t ram_PIX [0x300]; // 0x400

        uint16_t V_BE; // 0x700
        uint16_t _0 [0x7];
        uint16_t CP_SP0; // 0x708
        uint16_t _1;
        uint16_t ram_GAIN; // 0x70a
        uint16_t _2 [0x5];

        uint16_t _3 [0x10]; // 0x710

        uint16_t Ta_PTAT; // 0x720
        uint16_t _4 [0x7];
        uint16_t CP_SP1; // 0x728
        uint16_t _5;
        uint16_t VDD_raw; // 0x72A
        uint16_t _6 [0x5];

        uint16_t _7 [0x10]; // 0x730
    } named_;
};

#endif // __MEMORY_MLX90640_H__
