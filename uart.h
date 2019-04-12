#include <stdio.h>

void uart_init();

int uart_write(void *data, int len);
int uart_read(void *data, int len);
int uart_putchar(char c, FILE *stream);
