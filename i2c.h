#include <stdint.h>
#include <stdlib.h>

#define I2C_READ 0x01

typedef struct i2c_msg {
  uint8_t addr;
  uint8_t flags;
  uint8_t *data;
  size_t len;
} i2c_msg;

typedef struct i2c_result {
  i2c_msg *msgs;
  size_t len;
  uint8_t status;
} i2c_result;

/*
  schedules a transmission on the i2c_bus
  
  * the contents of msgs can be freed after the function returns
  * The result is freed after the callback is executed. Any desired data must
    be copied out.
*/
void i2c_send(i2c_msg *msgs, size_t len, void (*callback) (i2c_result*,void*), void *userdata);

