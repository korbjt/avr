#include <stdint.h>

#define BMP085_ADDR 0x77

typedef struct bmp085 {
  uint8_t addr;
  uint8_t oss;

  int16_t ac1;
  int16_t ac2;
  int16_t ac3;
  uint16_t ac4;
  uint16_t ac5;
  uint16_t ac6;
  int16_t b1;
  int16_t b2;
  int16_t mb;
  int16_t mc;
  int16_t md;

  int32_t ut;
  int32_t up;

  void (*callback) (struct bmp085*);
} bmp085;

bmp085* bmp085_init(uint8_t addr, void (*callback) (bmp085*));

float temp_c(bmp085 *bmp);
float temp_f(bmp085 *bmp);
float pressure(bmp085 *bmp);
