#include "uart.h"
#include "job.h"

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

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

void main(void) {
  uart_init();

  sei();

  FILE uart_out = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = &uart_out;

  job_sched(read_uart, 0, JOB_PERSIST);

  job_run();
}
