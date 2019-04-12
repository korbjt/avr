struct buffer;

struct buffer *buffer_alloc(int size);
void buffer_free(struct buffer *buf);
int buffer_write(struct buffer *buf, void *data, int len);
int buffer_read(struct buffer *buf, void *data, int len);
int buffer_len(struct buffer *buf);
