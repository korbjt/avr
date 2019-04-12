#include <avr/io.h>
#include <util/delay.h>

/* 
This program blinks PORTB5, which corresponds to pin 19 on the on 328p, and pin
13 on the Arduino Uno (which also has an LED on the board).

The _BV(n) macro left shifts 1 by n. The following statements are equivalent:
PORTB |= _BV(PORTB5)
PORTB |= 0b00100000
*/

int main() {
  // Set PORTB5 to output using the data direction register.
  DDRB |= _BV(DDB5);
  
  while(1) {
    PORTB |= _BV(PORTB5);
    _delay_ms(500);

    PORTB &= ~_BV(PORTB5);
    _delay_ms(500);
  }
}
