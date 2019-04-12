#include "uart.h"
#include "job.h"
#include "timer.h"
#include "bmp_085.h"

#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

void read_uart(void *userdata) {
  char buf[32];
  int n = uart_read(buf, 32);
  for(int i = 0; i < n; i++) {
    uart_write(&buf[i], 1);
    if(buf[i] == '\r') {
      uart_write("\n", 1);
    }
  }
}

volatile uint32_t samples = 0;
void bmp085_complete(bmp085 *bmp) {
  samples++;
}

void print_bmp(void *userdata) {
  struct bmp085 *bmp = userdata;
  printf("Temp (F): %f\n", temp_f(bmp));
  float pr = pressure(bmp);
  double elevation = 44330.0f * (1.0f - pow(pr/101325, (1.0f / 5.255f)));
  printf("Pressure (Pa): %f\n", pr);
  printf("Elevation (m): %f\n", elevation);
  printf("samples: %ld\n", samples);
}

void main(void) {
  uart_init();
  timer_init();

  TWSR = 0;
  TWBR = 72;

  sei();

  FILE uart_out = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = &uart_out;

  printf("starting timer\n");
  job_sched(read_uart, 0, JOB_PERSIST);

  struct bmp085 *bmp = bmp085_init(BMP085_ADDR, bmp085_complete);
  timer_sched(5000, print_bmp, bmp, TIMER_PERSIST);

  job_run();
}
