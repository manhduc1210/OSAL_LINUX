#pragma once
#include <stdint.h>

void BoardLed_Init(void);
void BoardLed_Set(uint8_t on);

// typedef struct {
//     const char* uio_path;   // ví dụ "/dev/uio0"
//     off_t       map_size;   // ví dụ 0x1000
// } OSAL_LinuxCtx;
