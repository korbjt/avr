#include "buffer.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct buffer {
  uint8_t *data;
  int r_idx;
  int w_idx;
  int size;
};

struct buffer *buffer_alloc(int size) {
  struct buffer *buf = (struct buffer *)malloc(sizeof(struct buffer));
  buf->data = malloc(size+1);
  buf->size = size+1;
  buf->r_idx = 0;
  buf->w_idx = 0;

  return buf;
}

void buffer_free(struct buffer *buf) {
  free((void *)buf->data);
  free(buf);
}

int buffer_write(struct buffer *buf, void *data, int len) {
  int delta = buf->r_idx - buf->w_idx;
  int available = delta > 0 ? delta - 1 : buf->size + delta - 1;
  if(available < 1) {
    return 0;
  }
  
  int n = len > available ? available : len;
  int end = (buf->w_idx + n) % buf->size;
  if(end < buf->w_idx) {
    int n1 = buf->size - buf->w_idx;
    int n2 = end;
    //write is wrapping
    memcpy(buf->data + buf->w_idx, data, n1);
    memcpy(buf->data, (uint8_t *)data + n1, n2);
  } else {
    memcpy(buf->data + buf->w_idx, data, n);
  }
  buf->w_idx = end;

  return n;
}

int buffer_read(struct buffer *buf, void *data, int len) {
  int delta = buf->w_idx - buf->r_idx;
  int available = delta >= 0 ? delta : buf->size + delta;
  if(available < 1) {
    return 0;
  }
  
  int n = len > available ? available : len;
  int end = (buf->r_idx + n) % buf->size;
  if(end < buf->r_idx) {
    int n1 = buf->size - buf->r_idx;
    int n2 = end;
    memcpy(data, buf->data + buf->r_idx, n1);
    memcpy((uint8_t *)data + n1, buf->data, n2);
  } else {
    memcpy(data, buf->data + buf->r_idx, n);
  }
  buf->r_idx = end;

  return n;
}

int buffer_len(struct buffer *buf) {
  int delta = buf->w_idx - buf->r_idx;
  return delta >= 0 ? delta : buf->size + delta;
}
