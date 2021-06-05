#ifndef __PUSH_DATA_HPP__
#define __PUSH_DATA_HPP__

#include <cstdint>
#include "mlx90640.hpp"

uint8_t * gst_get_userp(void);
bool gst_arm_buffer(const mlx90640::pixel pix_list[3]);

int gst_init_(int, int);
void gst_start_running(void);
void gst_cleanup(void);

#endif // __PUSH_DATA_HPP__
