#include "i2c.h"
#include "job.h"

#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/twi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct i2c_tx {
  i2c_msg *msgs;
  size_t len;
  void (*callback) (i2c_result*, void*);
  void *userdata;
  size_t msg_idx;
  size_t data_idx;
  
  struct i2c_tx *next;
};


static struct i2c_tx * volatile head = 0;
static struct i2c_tx * volatile tail = 0;

static i2c_msg* copy_msgs(i2c_msg *msgs, size_t len) {
  i2c_msg *dst = (i2c_msg*)malloc(len * sizeof(i2c_msg));
  if(!dst) {
    return 0;
  }

  for(int i = 0; i < len; i++) {
    // copy the struct
    dst[i] = msgs[i];

    // copy the msg buffer
    dst[i].data = (uint8_t*)malloc(msgs[i].len * sizeof(uint8_t));
    memcpy(dst[i].data, msgs[i].data, msgs[i].len);
  }

  return dst;
}

static void callback(void *userdata) {
  struct i2c_tx *tx = (struct i2c_tx*)userdata;

  i2c_result *result = (i2c_result *)malloc(sizeof(i2c_result));
  if(!result) {
    return;
  }
  result->msgs = tx->msgs;
  result->len = tx->len;
  result->status = (tx->msg_idx < tx->len) ? 1 : 0;

  tx->callback(result, tx->userdata);

  // free each message in the transaction
  for(int i = 0; i < tx->len; i++) {
    free(tx->msgs[i].data);
  }
  free(tx->msgs);
  free(tx);
  free(result);
}

static int tx_complete() {
  struct i2c_tx *tx = head;

  head = head->next;
  if(!head) {
    tail = 0;
  }

  job_sched(callback, tx, 0);

  return (head != 0);
}

void i2c_send(i2c_msg *msgs, size_t len, void (*callback) (i2c_result*, void *userdata), void *userdata) {
  struct i2c_tx *tx = (struct i2c_tx*)malloc(sizeof(struct i2c_tx));
  if(!tx) {
    return;
  }

  tx->msgs = copy_msgs(msgs, len);
  tx->len = len;
  tx->callback = callback;
  tx->userdata = userdata;
  tx->msg_idx = 0;
  tx->data_idx = 0;
  tx->next = 0;

  if(tx->msgs == 0) {
    return;
  }

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if(tail) {
      tail->next = tx;
      tail = tx;
    } else {
      head = tx;
      tail = tx;
    }

    if(TW_STATUS == 0xF8) {
      if(!(TWCR & _BV(TWSTO))) {
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);        
      }
    }
  }
}

ISR(TWI_vect) {
  struct i2c_tx *tx = head;
  if(!tx) {
    TWCR = 0;
    return;
  }
  i2c_msg *msg = &tx->msgs[tx->msg_idx];

  switch (TW_STATUS) {
  case TW_START:
  case TW_REP_START:
    // init data index for message
    tx->data_idx = 0;

    uint8_t addr;
    if(msg->flags & I2C_READ) {
      addr = (msg->addr << 1) | TW_READ;
    } else {
      addr = (msg->addr << 1) | TW_WRITE;
    }

    TWDR = addr;
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
    break;
  case TW_MT_SLA_ACK:
  case TW_MT_DATA_ACK:
    if(tx->data_idx < msg->len) { // more data to transmit
      uint8_t data = msg->data[tx->data_idx++];
      TWDR = data;
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
    } else if(++tx->msg_idx < tx->len) { // another message follows the current one
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
    } else {
      if(tx_complete()) {
        // another transaction is ready to go
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO) | _BV(TWSTA);
      } else {
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO);
      }
    }
    break;
  case TW_MT_SLA_NACK:
  case TW_MT_DATA_NACK:
  case TW_MR_SLA_NACK:
    if(tx_complete()) {
      // another transaction is ready to go
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO) | _BV(TWSTA);
    } else {
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO);
    }
    break;
  case TW_MT_ARB_LOST: // == TW_MR_ARB_LOST
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
    break;
  case TW_MR_DATA_ACK:
    msg->data[tx->data_idx++] = TWDR;
    // fallthrough to SLA ACK to handle the next n/ack
  case TW_MR_SLA_ACK:
    if(tx->data_idx < msg->len-1) { // more to receive
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
    } else {
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
    }
    break;
  case TW_MR_DATA_NACK:
    msg->data[tx->data_idx] = TWDR;

    if(++tx->msg_idx < tx->len) { // another message follows the current one
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
    } else {
      if(tx_complete()) {
        // another transaction is ready to go
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO) | _BV(TWSTA);
      } else {
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTO);
      }
    }        
    break;
  default:
    TWCR = 0;
    break;
  }
}


