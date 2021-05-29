#ifndef __PUSH_DATA_HPP__
#define __PUSH_DATA_HPP__

#include <cstdint>

uint8_t * gst_get_userp(void);
bool gst_arm_buffer();

int gst_init_(int, int);
void gst_start_running(void);
void gst_cleanup(void);

#endif // __PUSH_DATA_HPP__
