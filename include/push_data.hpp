#ifndef __PUSH_DATA_HPP__
#define __PUSH_DATA_HPP__

#include <cstdint>

uint16_t* get_userp(void);
bool arm_buffer(void);

int gst_init(void);
void gst_start_running(void);
void gst_cleanup(void);

#endif // __PUSH_DATA_HPP__
