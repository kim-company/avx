#include <demuxer.h>

// File-based demuxer.

int demuxer_alloc_from_file(Demuxer **ctx, char *path) {
  AVFormatContext *fmt_ctx;
  Demuxer *demuxer;
  int errn;

  if (!(fmt_ctx = avformat_alloc_context()))
    return AVERROR(ENOMEM);

  if ((errn = avformat_open_input(&fmt_ctx, path, NULL, NULL)) < 0)
    return errn;

  if ((errn = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    goto fail;

  if (!(demuxer = (Demuxer *)malloc(sizeof(Demuxer))))
    return AVERROR(ENOMEM);

  demuxer->fmt_ctx = fmt_ctx;
  *ctx = demuxer;
  return 0;

fail:
  avformat_close_input(&fmt_ctx);
  *ctx = NULL;
  return errn;
}

int demuxer_read_packet(Demuxer *ctx, AVPacket *packet) {
  return av_read_frame(ctx->fmt_ctx, packet);
}

void demuxer_free(Demuxer **ctx) {
  avformat_close_input(&(*ctx)->fmt_ctx);
  free(*ctx);
  *ctx = NULL;
}
