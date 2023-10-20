#include <sys/types.h>

typedef enum { QUEUE_MODE_SHIFT, QUEUE_MODE_GROW } QUEUE_MODE;

typedef struct {
  void *ptr;
  // The total size of ptr
  u_long size;
  // Where the next bytes should be written at.
  u_long buf_end;

  // position of the last read.
  u_long pos;

  // Used to differentiate wether the queue is removing
  // the bytes each time they're read or it is growing to
  // accomodate more. The latter is used when probing the
  // input to find the header.
  QUEUE_MODE mode;
  int input_eos;
} Ioq;

int queue_is_filled(Ioq *q);
int queue_freespace(Ioq *q);
void queue_grow(Ioq *q, int factor);
void queue_copy(Ioq *q, void *src, int size);
void queue_deq(Ioq *q);
int queue_read(Ioq *q, void *dst, int buf_size);
