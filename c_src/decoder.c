#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
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

int alloc_resampler(Decoder *ctx, FormatOpts output_opts) {
  AVChannelLayout *output_layout;
  AVCodecContext *codec_ctx;
  int errn;

  codec_ctx = ctx->codec_ctx;

  output_layout = malloc(sizeof(AVChannelLayout));
  av_channel_layout_copy(output_layout, &ctx->codec_ctx->ch_layout);
  output_layout->nb_channels = output_opts.channels;

  ctx->output_ch_layout = output_layout;

  output_opts.sample_format =
      av_get_packed_sample_fmt(output_opts.sample_format);

  if ((errn = swr_alloc_set_opts2(
           &(ctx->resampler_ctx), output_layout, output_opts.sample_format,
           output_opts.sample_rate, &codec_ctx->ch_layout,
           codec_ctx->sample_fmt, codec_ctx->sample_rate, 0, NULL)) != 0)
    return errn;

  ctx->output_fmt = malloc(sizeof(FormatOpts));
  *ctx->output_fmt = output_opts;

  return swr_init(ctx->resampler_ctx);
}

int decoder_alloc(Decoder **ctx, DecoderOpts opts) {
  const AVCodec *codec;
  AVCodecContext *codec_ctx;
  Decoder *ictx;
  int errn;

  codec = avcodec_find_decoder((enum AVCodecID)opts.codec_id);
  codec_ctx = avcodec_alloc_context3(codec);

  if ((errn = avcodec_parameters_to_context(codec_ctx, opts.params)) < 0)
    return errn;

  if ((errn = avcodec_open2(codec_ctx, codec, NULL)) < 0)
    return errn;

  codec_ctx->pkt_timebase = opts.timebase;

  ictx = (Decoder *)malloc(sizeof(Decoder));
  ictx->codec_ctx = codec_ctx;
  if ((errn = alloc_resampler(ictx, opts.output_opts)) < 0)
    return errn;

  *ctx = ictx;
  return 0;
}

int decoder_send_packet(Decoder *ctx, AVPacket *packet) {
  return avcodec_send_packet(ctx->codec_ctx, packet);
}

int decoder_read_frame(Decoder *ctx, AVFrame *frame) {
  int errn;

  if ((errn = avcodec_receive_frame(ctx->codec_ctx, frame)) != 0)
    return errn;

  if (ctx->resampler_ctx) {
    int next_pts = swr_next_pts(ctx->resampler_ctx, frame->pts);

    AVFrame *resampled_frame;
    resampled_frame = av_frame_alloc();
    resampled_frame->nb_samples = frame->nb_samples;
    resampled_frame->ch_layout = *ctx->output_ch_layout;
    resampled_frame->sample_rate = frame->sample_rate;
    resampled_frame->format = ctx->output_fmt->sample_format;

    if ((errn = av_frame_get_buffer(resampled_frame, 0)) != 0)
      return errn;

    if ((errn = swr_convert_frame(ctx->resampler_ctx, resampled_frame,
                                  frame)) != 0)
      return errn;

    resampled_frame->pts = next_pts;

    av_frame_unref(frame);
    av_frame_ref(frame, resampled_frame);
  }

  return 0;
}

int decoder_free(Decoder **ctx) {
  avcodec_free_context(&(*ctx)->codec_ctx);
  if ((*ctx)->resampler_ctx) {
    swr_free(&(*ctx)->resampler_ctx);
    free((*ctx)->output_fmt);
  }

  free(*ctx);
  return 0;
}
