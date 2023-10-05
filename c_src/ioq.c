#include <ioq.h>
#include <stdlib.h>
#include <string.h>

int queue_is_filled(Ioq *q) { return q->buf_end == q->size; }
int queue_freespace(Ioq *q) { return q->size - q->buf_end; }

void queue_grow(Ioq *q, int factor) {
  u_long new_size;

  new_size = q->size * factor;
  q->ptr = realloc(q->ptr, new_size);
  q->size = new_size;
}

void queue_copy(Ioq *q, void *src, int size) {
  // Do we have enough space for the data? If not, reallocate some space.
  if (queue_freespace(q) < size)
    queue_grow(q, 2);

  memcpy(q->ptr + q->buf_end, src, size);
  q->buf_end += size;
}

void queue_deq(Ioq *q) {
  int unread;

  unread = q->buf_end - q->pos;
  if (unread == 0) {
    q->pos = 0;
    q->buf_end = 0;
  } else {
    memmove(q->ptr, q->ptr + q->pos, unread);
    q->pos = 0;
    q->buf_end = unread;
  }
}

int queue_read(Ioq *q, void *dst, int buf_size) {
  int unread;
  int size;

  unread = q->buf_end - q->pos;
  if (unread <= 0)
    return -1;

  size = buf_size > unread ? unread : buf_size;
  memcpy(dst, q->ptr + q->pos, size);
  q->pos += size;

  if (q->mode == QUEUE_MODE_SHIFT)
    queue_deq(q);

  return size;
}
