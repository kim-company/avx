#include <decoder.h>

int is_planar(enum AVSampleFormat fmt) {
  switch (fmt) {
  case AV_SAMPLE_FMT_U8P:
  case AV_SAMPLE_FMT_S16P:
  case AV_SAMPLE_FMT_S32P:
  case AV_SAMPLE_FMT_FLTP:
  case AV_SAMPLE_FMT_DBLP:
  case AV_SAMPLE_FMT_S64P:
    return 1;
  default:
    return 0;
  }
}

int alloc_resampler(Decoder *ctx) {
  int ret;
  AVCodecContext *codec_ctx;
  enum AVSampleFormat output_fmt;

  codec_ctx = ctx->codec_ctx;
  output_fmt = av_get_packed_sample_fmt(codec_ctx->sample_fmt);

  swr_alloc_set_opts2(&(ctx->resampler_ctx), &codec_ctx->ch_layout, output_fmt,
                      codec_ctx->sample_rate, &codec_ctx->ch_layout,
                      codec_ctx->sample_fmt, codec_ctx->sample_rate, 0, NULL);

  ctx->output_sample_format = output_fmt;
  return swr_init(ctx->resampler_ctx);
}

int decoder_alloc(Decoder **ctx, enum AVCodecID codec_id,
                  AVCodecParameters *params) {
  const AVCodec *codec;
  AVCodecContext *codec_ctx;
  Decoder *ictx;
  int errn;

  codec = avcodec_find_decoder((enum AVCodecID)codec_id);
  codec_ctx = avcodec_alloc_context3(codec);

  if ((errn = avcodec_parameters_to_context(codec_ctx, params)) < 0)
    return errn;

  if ((errn = avcodec_open2(codec_ctx, codec, NULL)) < 0)
    return errn;

  ictx = (Decoder *)malloc(sizeof(Decoder));
  ictx->codec_ctx = codec_ctx;
  ictx->output_sample_format = codec_ctx->sample_fmt;

  // Resampler is here to play well with the Membrane framework, which
  // does not support planar PCM.
  if (is_planar(codec_ctx->sample_fmt))
    alloc_resampler(ictx);

  *ctx = ictx;
  return 0;
}

int decoder_send_packet(Decoder *ctx, AVPacket *packet) {
  return avcodec_send_packet(ctx->codec_ctx, packet);
}

int decoder_read_frame(Decoder *ctx, AVFrame *frame) {
  int errn;
  // AVFrame *oframe;

  if ((errn = avcodec_receive_frame(ctx->codec_ctx, frame)) != 0)
    return errn;

  if (ctx->resampler_ctx) {
    AVFrame *resampled_frame;
    resampled_frame = av_frame_alloc();
    resampled_frame->nb_samples = frame->nb_samples;
    resampled_frame->ch_layout = frame->ch_layout;
    resampled_frame->sample_rate = frame->sample_rate;
    resampled_frame->format = ctx->output_sample_format;

    if ((errn = av_frame_get_buffer(resampled_frame, 0)) != 0)
      return errn;

    if ((errn = swr_convert_frame(ctx->resampler_ctx, resampled_frame,
                                  frame)) != 0)
      return errn;
    resampled_frame->pts = frame->pts;

    av_frame_unref(frame);
    av_frame_ref(frame, resampled_frame);
  }

  return 0;
}

int decoder_free(Decoder **ctx) {
  avcodec_free_context(&(*ctx)->codec_ctx);
  if ((*ctx)->resampler_ctx)
    swr_free(&(*ctx)->resampler_ctx);

  free(*ctx);
  return 0;
}
