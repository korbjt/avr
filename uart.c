#include "buffer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdio.h>
#include <util/atomic.h>
#include <util/setbaud.h>

static struct buffer *read_buf;
static struct buffer *write_buf;

static volatile int write_ready;

void uart_init() {
  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;

  UCSR0B |= _BV(TXEN0) | _BV(RXEN0) | _BV(RXCIE0);

  // set frame format (8 data, 2 stop): UCSR0C
  UCSR0C |= _BV(USBS0) | _BV(UCSZ00) | _BV(UCSZ01);

  write_buf = buffer_alloc(512);
  read_buf = buffer_alloc(64);
}

int uart_write(void *data, int len) {
  int n = 0;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    n = buffer_write(write_buf, data, len);
    if(write_ready) {
      char c;
      buffer_read(write_buf, &c, 1);
      UDR0 = c;

      write_ready = 0;
    }
    
    //enable UDRE0 interrupt
    UCSR0B |= _BV(UDRIE0);
  }
  return n;
}

int uart_read(void *data, int len) {
  int n = 0;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    n = buffer_read(read_buf, data, len);
  }
  return n;
}

int uart_putchar(char c, FILE *stream) {
  if(c == '\n') {
    uart_write("\r\n", 2);
    return 0;
  }

  uart_write(&c, 1);
  return 0;
}

ISR(USART_RX_vect) {
  char c = UDR0;
  buffer_write(read_buf, &c, 1);
}

ISR(USART_UDRE_vect) {
  char c;
  int read = buffer_read(write_buf, &c, 1);
  if(read > 0) {
    UDR0 = c;
  } else {
    write_ready = 1;
    
    //disable the interrupt
    UCSR0B &= ~_BV(UDRIE0);
  }
}
