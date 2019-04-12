#include "bmp_085.h"
#include "i2c.h"
#include "timer.h"

static int16_t read_16(uint8_t *data) {
  int16_t val = *data;
  val = (val << 8) | *(data+1);
  return val;
}

static uint16_t read_u16(uint8_t *data) {
  uint16_t val = *data;
  val = (val << 8) | *(data+1);
  return val;
}

static void temp_init_start(bmp085 *bmp);

static void restart(void *userdata) {
  temp_init_start((bmp085*)userdata);
}

static void pres_read_done(i2c_result *result, void *userdata) {
  bmp085 *bmp = (bmp085 *)userdata;
  if(result->status) {
    timer_sched(10, restart, bmp, 0);
    return;
  }

  bmp->callback(bmp);
  i2c_msg *msg = &result->msgs[1];

  bmp->up = 0;
  bmp->up = (bmp->up << 8) | msg->data[0];
  bmp->up = (bmp->up << 8) | msg->data[1];
  bmp->up = (bmp->up << 8) | msg->data[2];
  bmp->up = bmp->up >> (8 - bmp->oss);

  restart(bmp);
}

static void pres_read_start(void *userdata) {
  if(userdata == 0) {
    return;
  }
  bmp085 *bmp = (bmp085 *)userdata;

  uint8_t reg[1] = {0xF6};
  uint8_t data[3];

  i2c_msg msgs[2] = {
    {
      .addr = bmp->addr,
      .len = 1,
      .data = reg,
    },
    {
      .addr = bmp->addr,
      .flags = I2C_READ,
      .len = 3,
      .data = data,
    }
  };

  i2c_send(msgs, 2, pres_read_done, bmp);
}

static void pres_init_done(i2c_result *result, void *userdata) {
  if(result->status) {
    return;
  }

  bmp085 *bmp = (bmp085 *)userdata;

  uint32_t delay;
  switch(bmp->oss) {
  case 1:
    delay = 8;
    break;
  case 2:
    delay = 15;
    break;
  case 3:
    delay = 26;
    break;
  default:
    delay = 5;
    break;
  }
  
  timer_sched(delay, pres_read_start, bmp, 0);
}

static void temp_read_done(i2c_result *result, void *userdata) {
  if(result->status) {
    return;
  }


  bmp085 *bmp = (bmp085 *)userdata;
  i2c_msg *msg = &result->msgs[1];

  bmp->ut = read_u16(&msg->data[0]);
  
  uint8_t send[2] = {0xF4, 0x34 + (bmp->oss << 6)};
  i2c_msg msgs[1] = {
    {
      .addr = bmp->addr,
      .len = 2,
      .data = send,
    }
  };

  i2c_send(msgs, 1, pres_init_done, bmp);
}

static void temp_read_start(void *userdata) {
  bmp085 *bmp = (bmp085 *)userdata;

  uint8_t reg[1] = {0xF6};
  uint8_t data[2];

  i2c_msg msgs[2] = {
    {
      .addr = bmp->addr,
      .len = 1,
      .data = reg,
    },
    {
      .addr = bmp->addr,
      .flags = I2C_READ,
      .len = 2,
      .data = data,
    }
  };

  i2c_send(msgs, 2, temp_read_done, bmp);
}

static void temp_init_done(i2c_result *result, void *userdata) {
  if(result->status) {
    return;
  }

  bmp085 *bmp = (bmp085 *)userdata;
  
  // sleep 5ms, then read
  timer_sched(5, temp_read_start, bmp, 0);
}

static void temp_init_start(bmp085 *bmp) {
  uint8_t send[2] = {0xF4, 0x2E};

  i2c_msg msgs[1] = {
    {
      .addr = bmp->addr,
      .len = 2,
      .data = send,
    }
  };

  i2c_send(msgs, 1, temp_init_done, bmp);
}

static void cal_read(i2c_result *result, void *userdata);

static void cal_start(void *userdata) {
  bmp085 *bmp = (bmp085 *)userdata;

  uint8_t reg[1] = {0xAA};
  uint8_t data[22];

  i2c_msg msgs[2] = {
    {
      .addr = bmp->addr,
      .len = 1,
      .data = reg,
    },
    {
      .addr = bmp->addr,
      .flags = I2C_READ,
      .len = 22,
      .data = data,
    }
  };

  i2c_send(msgs, 2, cal_read, bmp);
}

static void cal_read(i2c_result *result, void *userdata) {
  if(result->status) {
    return;
  }


  bmp085 *bmp = (bmp085 *)userdata;
  i2c_msg *msg = &result->msgs[1];

  bmp->ac1 = read_16(&msg->data[0]);
  bmp->ac2 = read_16(&msg->data[2]);
  bmp->ac3 = read_16(&msg->data[4]);
  bmp->ac4 = read_u16(&msg->data[6]);
  bmp->ac5 = read_u16(&msg->data[8]);
  bmp->ac6 = read_u16(&msg->data[10]);
  bmp->b1 = read_16(&msg->data[12]);
  bmp->b2 = read_16(&msg->data[14]);
  bmp->mb = read_16(&msg->data[16]);
  bmp->mc = read_16(&msg->data[18]);
  bmp->md = read_16(&msg->data[20]);

  temp_init_start(bmp);
}

bmp085* bmp085_init(uint8_t addr, void (*callback) (bmp085*)) {
  bmp085 *bmp = (bmp085 *)malloc(sizeof(bmp085));
  bmp->addr = addr;
  bmp->callback = callback;
  bmp->oss = 3;

  cal_start(bmp);

  return bmp;
}

static float ctof(float c) {
  return (1.8 * c) + 32;
}

float temp_f(bmp085 *bmp) {
  return ctof(temp_c(bmp));
}

float temp_c(bmp085 *b) {

  int32_t x1 = b->ut - b->ac6;
  x1 = x1 * b->ac5;
  x1 = x1 / 32768;

  int32_t x2 = b->mc;
  x2 = x2 * 2048;
  x2 = x2 / (x1 + b->md);

  int32_t b5 = x1 + x2;
  
  return (float)((b5 + 8) / 16) / 10.0f;
}

float pressure(bmp085 *b) {
  int32_t x1 = b->ut - b->ac6;
  x1 = x1 * b->ac5;
  x1 = x1 / 32768;

  int32_t x2 = b->mc;
  x2 = x2 * 2048;
  x2 = x2 / (x1 + b->md);

  int32_t b5 = x1 + x2;

  int32_t b6 = b5 - 4000;
  x1 = (b->b2 * ((b6 * b6) / 4096)) / 2048;
  x2 = (b->ac2 * b6) / 2048;
  
  int32_t x3 = x1 + x2;

  int32_t b3 = b->ac1;
  b3 = (b3 * 4) + x3;
  b3 = b3 << b->oss;
  b3 = (b3 + 2) / 4;
  
  x1 = (b->ac3 * b6) / 8192;
  x2 = (b->b1 * ((b6 * b6) / 4096)) / 65536;
  x3 = ((x1 + x2) + 2) / 4;
  
  uint32_t b4 = b->ac4 * (uint32_t)(x3 + 32768) / 32768;
  uint32_t b7 = ((uint32_t)b->up - b3) * (50000 >> b->oss);

  int32_t p = b7 < 0x80000000 ? (b7 * 2) / b4 : (b7 / b4) * 2;
  x1 = (((p / 256) * (p / 256)) * 3038) / 65536;
  x2 = (-7357 * p) / 65536;
  return p + (x1 + x2 + 3791) / 16;
}
