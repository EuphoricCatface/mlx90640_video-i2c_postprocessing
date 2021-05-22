#include "mlx90640.hpp"

void mlx90640::print_ee(void) {
    for (int i=0; i<0x340; i++)
    {
        char buffer[5];
        std::snprintf(buffer, 5, "%04hX", ee.word_[i]);
        std::cout << buffer;
        if (i % 16 == 15)
            std::cout << "\n";
        else
            std::cout << " ";
    }
}

bool mlx90640::read_ee(const char * path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1)
        return false;
    ssize_t rdsz_ = read(fd, (unsigned char *)(ee.word_),
        sizeof(ee) / sizeof(char));
    close(fd);
    std::cout << "EE: " << rdsz_ << " bytes read\n";
    return true;
}

unsigned short mlx90640::fetch_EE_address(int address) {
    const int OFFSET = 0x2400;

    if (address < OFFSET || address >= OFFSET + 0x340) {
        printf("bad EE addr, %d.\n", address);
        return 0;
    }
    return le16toh(ee.word_[address - OFFSET]);
}

bool mlx90640::init_ee(const char * path, bool ignore_ee_check) {
    bool rtn = read_ee(path);
    if (rtn == false) {
        printf("NVMEM read error\n");
        exit(EXIT_FAILURE);
    }

    if (!ignore_ee_check
            && ( memcmp(MLX_ID, &(ee.word_[0x07]), sizeof(MLX_ID) - 1)
                || memcmp(REG_CONF_EE, &(ee.word_[0x0C]), sizeof(REG_CONF_EE) - 1) )
    ) {
        printf("Error: Device ID or Register configuration does not match.\n");
        printf("First 16 words [0x2400:0x240F]:\n");
        for (int i = 0; i < 0x10; i++)
            printf("%04hX ", ee.word_[i]);
        printf("\n");
        printf("Example output:\n");
        printf("00A2 699F 0000 2061 0005 0320 03E0 1728 8E4F 0187 048D 0000 1901 0000 0000 BE33\n");
        printf("Possible reasons:\n");
        printf("1) You have changed the configuration [0x240C:0x240F]\n");
        printf("2) The ID of your device [0x2407:0x2409] is not known to the developer\n");
        printf("3) I2C baud rate is set too fast for EEPROM operation\n");
        printf("   (According to the datasheet, it has to be no faster than 0.4MHz)\n"); // Reading at 1MHz seems to be okay though
        printf("4) Your device is not MLX90640\n");
        printf("- If you believe the case is 1) or 2), try adding '--ignore_EE_check' option\n");
        printf("- If 3) is the case, dump NVMEM with lower baud rate and then use the dumped file to run this program\n");
        return false;
    }

    union {
        uint16_t word_;
        struct bitfield_ee2433 {
            unsigned int Vdd_25_EE : 8;
            int K_Vdd_EE : 8;
        } bf;
    } ee2433;
    ee2433.word_ = fetch_EE_address(0x2433);

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

    union {
        uint16_t word_;
        struct bitfield_ee2432 {
            unsigned int K_T_PTAT_l8: 8;
            unsigned int K_T_PTAT_u2: 2;
            int K_V_PTAT_EE: 6;
        } bf;
    } ee2432;
    ee2432.word_ = fetch_EE_address(0x2432);

    union {
        uint16_t word_;
        struct {
            int8_t K_V_rOcO: 4; // datasheet says Even Even, but that's 1-based index
            int8_t K_V_rEcO: 4;
            int8_t K_V_rOcE: 4;
            int8_t K_V_rEcE: 4;
        } bf;
    } ee2434;
    ee2434.word_ = fetch_EE_address(0x2434);

    union {
        uint16_t word_;
        struct {
            uint8_t _1 : 4;
            uint8_t K_V_scale : 4;
            uint8_t K_Ta_scale1 : 4;
            uint8_t K_Ta_scale2 : 4;
        } bf;
    } ee2438;
    ee2438.word_ = fetch_EE_address(0x2438);

    union {
        uint16_t word_;
        struct {
            int8_t K_Ta_rOcO: 8; // datasheet says Even Even, but that's 1-based index
            int8_t K_Ta_rEcO: 8;
        } bf;
    } ee2437;
    union {
        uint16_t word_;
        struct {
            int8_t K_Ta_rOcE: 8;
            int8_t K_Ta_rEcE: 8;
        } bf;
    } ee2436;
    ee2437.word_ = fetch_EE_address(0x2437);
    ee2436.word_ = fetch_EE_address(0x2436);

    union {
        uint16_t word_;
        struct bitfield_ee2420 {
            unsigned int scale_Acc_rem: 4;
            unsigned int scale_Acc_col: 4;
            unsigned int scale_Acc_row: 4;
            unsigned int a_scale_EE: 4;
        } bf;
    } ee2420;
    ee2420.word_ = fetch_EE_address(0x2420);

    union {
        uint16_t word_;
        struct {
            int8_t TGC: 8;
            int8_t KsTa_EE: 8;
        } bf;
    } ee243C;
    ee243C.word_ = fetch_EE_address(0x243C);

    Vdd_25_EE = ee2433.bf.Vdd_25_EE;
    K_Vdd_EE = ee2433.bf.K_Vdd_EE;

    a_PTAT = (double)ee2410.bf.a_PTAT_EE / 4.0 + 8.0;

    K_V_PTAT = (double)ee2432.bf.K_V_PTAT_EE / (double)(1 << 12);
    int K_T_PTAT_EE = (ee2432.bf.K_T_PTAT_u2 << 8)
                        | (ee2432.bf.K_T_PTAT_l8);
    K_T_PTAT = (double)K_T_PTAT_EE / 8.0;

    V_PTAT_25 = ee.named.PTAT_25;

    gain_ee = ee.named.ee_GAIN;

    // printf(" == K_V <2x2> == \n");
    unsigned K_V_scale = ee2438.bf.K_V_scale;

    K_V[0][0] = (double)ee2434.bf.K_V_rEcE / (double)(1 << K_V_scale);
    K_V[0][1] = (double)ee2434.bf.K_V_rEcO / (double)(1 << K_V_scale);
    K_V[1][0] = (double)ee2434.bf.K_V_rOcE / (double)(1 << K_V_scale);
    K_V[1][1] = (double)ee2434.bf.K_V_rOcO / (double)(1 << K_V_scale);

    // printf(" == K_Ta <frame> == \n");
    int K_Ta_PIX[0x300];
    for (int i=0; i < 0x300; i++) {
        K_Ta_PIX[i] = ee.named.ee_PIX[i].bf.K_Ta;
    }

    unsigned K_Ta_scale1 = ee2438.bf.K_Ta_scale1 + 8;
    unsigned K_Ta_scale2 = ee2438.bf.K_Ta_scale2;

    int K_Ta_2x2[2][2];
    K_Ta_2x2[0][0] = ee2436.bf.K_Ta_rEcE;
    K_Ta_2x2[1][0] = ee2436.bf.K_Ta_rOcE;
    K_Ta_2x2[0][1] = ee2437.bf.K_Ta_rEcO;
    K_Ta_2x2[1][1] = ee2437.bf.K_Ta_rOcO;

    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 32; col++) {
            int K_Ta_int = K_Ta_2x2[row%2][col%2] + (K_Ta_PIX[row * 32 + col] << K_Ta_scale2);
            K_Ta[row * 24 + col]
                = (double)(K_Ta_int)
                  / (double)(1 << K_Ta_scale1);
        }
    }

    // printf(" == offset == \n");
    int offset_avg = ee.named.PIX_OS_AVG;
    unsigned offset_row_scale = ee2410.bf.scale_Occ_row;
    unsigned offset_col_scale = ee2410.bf.scale_Occ_col;
    unsigned offset_rem_scale = ee2410.bf.scale_Occ_rem;
    int offset_row[24];
    int offset_col[32];
    for (int row_ = 0; row_ < 24/4; row_++) {
        offset_row[4*row_] = ee.named.OCC_ROW[row_].bf.OCC_ROW_1_;
        offset_row[4*row_+1] = ee.named.OCC_ROW[row_].bf.OCC_ROW_2_;
        offset_row[4*row_+2] = ee.named.OCC_ROW[row_].bf.OCC_ROW_3_;
        offset_row[4*row_+3] = ee.named.OCC_ROW[row_].bf.OCC_ROW_4_;
    }
    for (int col_ = 0; col_ < 32/4; col_++) {
        offset_col[4*col_] = ee.named.OCC_COL[col_].bf.OCC_COL_1_;
        offset_col[4*col_+1] = ee.named.OCC_COL[col_].bf.OCC_COL_2_;
        offset_col[4*col_+2] = ee.named.OCC_COL[col_].bf.OCC_COL_3_;
        offset_col[4*col_+3] = ee.named.OCC_COL[col_].bf.OCC_COL_4_;
    }
    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 32; col++) {
            offset_ref[row * 32 + col] =
                offset_avg +
                (offset_row[row] << offset_row_scale) +
                (offset_col[col] << offset_col_scale) +
                (ee.named.ee_PIX[row * 32 + col].bf.PIX_OFF << offset_rem_scale);
        }
    }

    // printf(" == alpha == \n");

    unsigned a_avg = ee.named.PIX_SENS_AVG;
    unsigned a_scale = ee2420.bf.a_scale_EE + 30;
    unsigned a_row_scale = ee2420.bf.scale_Acc_row;
    unsigned a_col_scale = ee2420.bf.scale_Acc_col;
    unsigned a_rem_scale = ee2420.bf.scale_Acc_rem;

    int a_row[24];
    int a_col[32];
    for (int row_ = 0; row_ < 24/4; row_++) {
        a_row[4*row_] = ee.named.ACC_ROW[row_].bf.ACC_ROW_1_;
        a_row[4*row_+1] = ee.named.ACC_ROW[row_].bf.ACC_ROW_2_;
        a_row[4*row_+2] = ee.named.ACC_ROW[row_].bf.ACC_ROW_3_;
        a_row[4*row_+3] = ee.named.ACC_ROW[row_].bf.ACC_ROW_4_;
    }
    for (int col_ = 0; col_ < 32/4; col_++) {
        a_col[4*col_] = ee.named.ACC_COL[col_].bf.ACC_COL_1_;
        a_col[4*col_+1] = ee.named.ACC_COL[col_].bf.ACC_COL_2_;
        a_col[4*col_+2] = ee.named.ACC_COL[col_].bf.ACC_COL_3_;
        a_col[4*col_+3] = ee.named.ACC_COL[col_].bf.ACC_COL_4_;
    }
    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 32; col++) {
            int a_rem = (ee.named.ee_PIX[row * 32 + col].word_ & 0x03f0) >> 4;
            if (a_rem & 1<<5)
                a_rem |= 0xffffffc0;
            int a_ref_int =
                a_avg +
                (a_row[row] << a_row_scale) +
                (a_col[col] << a_col_scale) +
                ((a_rem) << a_rem_scale);
            a_ref[row * 32 + col] = (double)a_ref_int / pow(2, a_scale);
        }
    }

    // printf(" == TGC check == \n");
    if (ee243C.bf.TGC)
        printf("Warning: TGC value present, which will be ignored\n");
    //TODO: detect interleave mode and inform the user how lazy of a programmer I am

    return true;
}

unsigned short mlx90640::fetch_RAM_address(int address) {
    const int OFFSET = 0x400;
    if (address < OFFSET || address >= OFFSET + 0x340) {
        printf("bad RAM addr, %d\n", address);
        return 0;
    }
    return le16toh(ram.word_[address - OFFSET]);
}

unsigned short mlx90640::fetch_reg_address(int address) {
    const int OFFSET = 0x8000;
    if (address < OFFSET || address >= OFFSET + 0x20) {
        printf("bad register addr, %d\n", address);
        return 0;
    }
    return le16toh(ram.word_[address - OFFSET + 0x340]);
}

void mlx90640::process_frame(void) {
    dV = (double)(-((int)Vdd_25_EE << 5) + VDD_raw + 16384) / (double) K_Vdd_EE / 32.0;
    V_PTAT_art = (double)(1 << 18) / (a_PTAT + (double)V_BE / (double)V_PTAT);
    dTa = (V_PTAT_art / (1.0 + K_V_PTAT * dV) - V_PTAT_25) / K_T_PTAT;
    gain = (double)gain_ee / (double)gain_ram;

    e = 1;
    T_ar = std::pow((dTa + 273.15 + 25.0), 4); // assuming emissivity is 1

    if (extended)
        subpage = fetch_reg_address(0x8000) % 2;
}

void mlx90640::process_pixel(void) {
    double t_min = HUGE_VAL;
    double t_max = -HUGE_VAL;

    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 32; col++) {
            int thispixel = row * 32 + col;
            if (!extended ||
                    (row + col) % 2 == subpage) {
                    // discrepancy from datasheet: datasheet is 1-based index
                    // also we're assuming checkerboard pattern
                pix[thispixel]
                    = (double)ram.named.ram_PIX[thispixel] * gain
                    - (double)offset_ref[thispixel]
                      * (1 + K_Ta[thispixel] * dTa)
                      * (1 + K_V[row%2][col%2] * dV);
                To[thispixel] = pow((pix[thispixel] / a_ref[thispixel] + T_ar), 0.25) - 273.15;
            }

            // min/max calculation has to be done whole frame regardless of subpage
            if (To[thispixel] < t_min) {
                t_min = To[thispixel];
                pix_list[MIN_T].x = col;
                pix_list[MIN_T].y = row;
                pix_list[MIN_T].T = To[thispixel];
            }

            if (To[thispixel] > t_max) {
                t_max = To[thispixel];
                pix_list[MAX_T].x = col;
                pix_list[MAX_T].y = row;
                pix_list[MAX_T].T = To[thispixel];
            }
        }
    }

    pix_list[SCENE_CENTER].x = 16;
    pix_list[SCENE_CENTER].y = 12;
    pix_list[SCENE_CENTER].T = To[12 * 32 + 16];
}
