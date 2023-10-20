#include "libavformat/avformat.h"
#include <demuxer.h>
#include <ioq.h>

// File-based demuxer.

typedef struct {
  AVFormatContext *fmt_ctx;
} DemuxerFile;

int demuxer_file_alloc(DemuxerFile **ctx, char *path) {
  AVFormatContext *fmt_ctx;
  DemuxerFile *demuxer;
  int errn;

  fmt_ctx = avformat_alloc_context();
  if ((errn = avformat_open_input(&fmt_ctx, path, NULL, NULL)) < 0)
    return errn;

  if ((errn = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    goto fail;

  demuxer = (DemuxerFile *)malloc(sizeof(DemuxerFile));
  demuxer->fmt_ctx = fmt_ctx;
  *ctx = demuxer;
  return 0;

fail:
  avformat_close_input(&fmt_ctx);
  *ctx = NULL;
  return errn;
}

int demuxer_file_read_packet(void *ctx, AVPacket *packet) {
  return av_read_frame(((DemuxerFile *)ctx)->fmt_ctx, packet);
}

void demuxer_file_fmt_ctx(void *opaque, AVFormatContext **fmt_ctx) {
  DemuxerFile *ctx = (DemuxerFile *)opaque;
  *fmt_ctx = ctx->fmt_ctx;
}

int default_add(void *ctx, void *data, int size) { return -1; }
int default_demand(void *ctx) { return -1; }
int default_is_ready(void *ctx) { return 1; }

void demuxer_file_free(void **ctx) {
  DemuxerFile **demuxer = (DemuxerFile **)ctx;
  avformat_close_input(&(*demuxer)->fmt_ctx);
  free(*ctx);
  *ctx = NULL;
}

// @warning In memory demuxer, experimental.

typedef enum { CTX_MODE_DRAIN, CTX_MODE_BUF } CTX_MODE;

typedef struct {
  // Stores incoming bytes. Allows rewinding.
  Ioq *queue;
  // The context responsible for reading data from the queue. It is
  // configured to use the read_packet function as source.
  AVIOContext *io_ctx;
  // The actual libAV demuxer.
  AVFormatContext *fmt_ctx;

  CTX_MODE mode;

  int has_header;
} DemuxerMem;

int demuxer_mem_alloc(DemuxerMem **demuxer, int probe_size) {
  DemuxerMem *ctx;

  Ioq *queue = (Ioq *)malloc(sizeof(Ioq));
  queue->ptr = malloc(probe_size);
  queue->mode = QUEUE_MODE_GROW;
  queue->input_eos = 0;
  queue->size = probe_size;
  queue->pos = 0;
  queue->buf_end = 0;

  ctx = (DemuxerMem *)malloc(sizeof(DemuxerMem));
  ctx->queue = queue;
  ctx->mode = CTX_MODE_BUF;
  ctx->has_header = 0;
  *demuxer = ctx;

  return 0;
}

int read_ioq(void *opaque, uint8_t *buf, int buf_size) {
  int size;
  Ioq *queue;

  queue = (Ioq *)opaque;

  if ((size = queue_read(queue, buf, buf_size)) < 0) {
    if (queue->input_eos)
      return AVERROR_EOF;
    // Avoid telling avio that the input is finished if
    // we're the input is not.
    return 0;
  }

  return size;
}

void demuxer_mem_fmt_ctx(void *opaque, AVFormatContext **fmt_ctx) {
  DemuxerMem *ctx = (DemuxerMem *)opaque;
  *fmt_ctx = ctx->fmt_ctx;
}

int demuxer_mem_read_header(DemuxerMem *ctx) {
  AVIOContext *io_ctx;
  AVFormatContext *fmt_ctx;
  void *io_buffer;
  int errnum;
  int ret;

  // TODO can we avoid allocating this buffer each time we do a read attempt?
  io_buffer = av_malloc(ctx->queue->size);
  // Context that reads from queue and uses io_buffer as scratch space.
  io_ctx = avio_alloc_context(io_buffer, ctx->queue->size, 0, ctx->queue,
                              &read_ioq, NULL, NULL);
  io_ctx->seekable = 0;

  fmt_ctx = avformat_alloc_context();
  fmt_ctx->pb = io_ctx;
  fmt_ctx->probesize = ctx->queue->size;

  if ((errnum = avformat_open_input(&fmt_ctx, NULL, NULL, NULL)))
    goto open_error;

  if ((errnum = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    goto open_error;

  ctx->has_header = 1;
  ctx->io_ctx = io_ctx;
  ctx->fmt_ctx = fmt_ctx;

  // From now on the queue will stop growing, it will rather
  // delete data once it is read as we do not need to read
  // it again from the beginning, we found the header!
  queue_deq(ctx->queue);
  ctx->queue->mode = QUEUE_MODE_SHIFT;

  return errnum;

open_error:
  avio_context_free(&io_ctx);
  avformat_close_input(&fmt_ctx);

  queue_grow(ctx->queue, 2);
  ctx->queue->pos = 0;

  return errnum;
}

int demuxer_mem_add_data(void *ctx, void *data, int size) {
  DemuxerMem *demuxer = (DemuxerMem *)ctx;

  // Indicates EOS.
  if (!data) {
    demuxer->mode = CTX_MODE_DRAIN;
    demuxer->queue->input_eos = 1;
    return 0;
  }

  // Copy the data in the buffer.
  queue_copy(demuxer->queue, data, size);

  // Make an attemp reading the header only when the ioq buffer is filled.
  if (!demuxer->has_header && queue_is_filled(demuxer->queue))
    demuxer_mem_read_header(ctx);

  return 0;
}

int demuxer_mem_is_ready(void *opaque) {
  return ((DemuxerMem *)opaque)->has_header;
}

int demuxer_mem_read_streams(void *opaque, AVStream **streams,
                             unsigned int *size) {
  int errn;
  DemuxerMem *ctx = (DemuxerMem *)opaque;

  // Called on EOS: we're not ready, meaning that we did not get
  // the amount of data we wanted, but we may still be able to
  // obtain the streams.
  if (!ctx->has_header) {
    if ((errn = demuxer_mem_read_header(ctx)) != 0) {
      return errn;
    }
  }

  *size = (int)ctx->fmt_ctx->nb_streams;
  *streams = *ctx->fmt_ctx->streams;
  return 0;
}

int demuxer_mem_read_packet(void *opaque, AVPacket *packet) {
  int errn, freespace;
  DemuxerMem *ctx;

  ctx = (DemuxerMem *)opaque;
  freespace = queue_freespace(ctx->queue);

  if (freespace > 0 && ctx->mode == CTX_MODE_BUF)
    // When we're not draining the demuxer, try to
    // keep the ioq filled with data.
    return freespace;

  return av_read_frame(ctx->fmt_ctx, packet);
}

int demuxer_mem_demand(void *opaque) {
  return queue_freespace(((DemuxerMem *)opaque)->queue);
}

void demuxer_mem_free(void **opaque) {
  DemuxerMem **ctx = (DemuxerMem **)opaque;

  free((*ctx)->queue->ptr);
  avio_context_free(&(*ctx)->io_ctx);
  avformat_close_input(&(*ctx)->fmt_ctx);
  free(*ctx);
  *opaque = NULL;
}

// Public API.

struct Demuxer {
  void *opaque;

  int (*read_packet)(void *, AVPacket *);
  int (*add_data)(void *, void *, int);
  int (*read_streams)(void *, AVStream ***, unsigned int *);
  int (*is_ready)(void *);
  int (*demand)(void *);
  void (*fmt_ctx)(void *, AVFormatContext **);
  void (*free)(void **);
};

int demuxer_alloc_from_file(Demuxer **demuxer, char *path) {
  DemuxerFile *ctx;
  Demuxer *idemuxer;
  int errn;

  if ((errn = demuxer_file_alloc(&ctx, path)) < 0)
    return errn;

  idemuxer = (Demuxer *)malloc(sizeof(Demuxer));
  idemuxer->opaque = ctx;
  idemuxer->read_packet = &demuxer_file_read_packet;
  idemuxer->add_data = &default_add;
  idemuxer->fmt_ctx = &demuxer_file_fmt_ctx;
  idemuxer->is_ready = &default_is_ready;
  idemuxer->free = &demuxer_file_free;
  idemuxer->demand = &default_demand;

  *demuxer = idemuxer;
  return 0;
}

int demuxer_alloc_in_mem(Demuxer **demuxer, int probe_size) {
  DemuxerMem *ctx;
  Demuxer *idemuxer;
  int errn;

  if ((errn = demuxer_mem_alloc(&ctx, probe_size)) < 0)
    return errn;

  idemuxer = (Demuxer *)malloc(sizeof(Demuxer));
  idemuxer->opaque = (void *)ctx;
  idemuxer->read_packet = &demuxer_mem_read_packet;
  idemuxer->add_data = &demuxer_mem_add_data;
  idemuxer->fmt_ctx = &demuxer_mem_fmt_ctx;
  idemuxer->is_ready = &demuxer_mem_is_ready;
  idemuxer->free = &demuxer_mem_free;
  idemuxer->demand = &demuxer_mem_demand;

  *demuxer = idemuxer;
  return 0;
}

int demuxer_read_packet(Demuxer *ctx, AVPacket *packet) {
  return ctx->read_packet(ctx->opaque, packet);
}

int demuxer_add_data(Demuxer *ctx, void *data, int size) {
  return ctx->add_data(ctx->opaque, data, size);
}

int demuxer_demand(Demuxer *ctx) { return ctx->demand(ctx->opaque); }

int demuxer_is_ready(Demuxer *ctx) { return ctx->is_ready(ctx->opaque); }

void demuxer_fmt_ctx(Demuxer *ctx, AVFormatContext **fmt_ctx) {
  ctx->fmt_ctx(ctx->opaque, fmt_ctx);
}

void demuxer_free(Demuxer **ctx) {
  (*ctx)->free(&(*ctx)->opaque);
  free(*ctx);
  *ctx = NULL;
}
