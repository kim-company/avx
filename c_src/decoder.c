#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include <decoder.h>
#include <libavutil/opt.h>

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
  AVCodecContext *codec_ctx;
  SwrContext *swr_ctx;
  int errn;

  codec_ctx = ctx->codec_ctx;
  if (!(swr_ctx = swr_alloc()))
    return AVERROR(ENOMEM);

  av_opt_set_chlayout(swr_ctx, "in_chlayout", &codec_ctx->ch_layout, 0);
  av_opt_set_int(swr_ctx, "in_sample_rate", codec_ctx->sample_rate, 0);
  av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0);

  av_opt_set_chlayout(swr_ctx, "out_chlayout", ctx->output.ch_layout, 0);
  av_opt_set_int(swr_ctx, "out_sample_rate", ctx->output.sample_rate, 0);
  av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", ctx->output.sample_format,
                        0);

  if ((errn = swr_init(swr_ctx)) < 0)
    return errn;

  ctx->resampler_ctx = swr_ctx;
  return errn;
}

int decoder_alloc(Decoder **ctx, DecoderOpts opts) {
  const AVCodec *codec;
  AVCodecContext *codec_ctx;
  Decoder *ictx;
  int errn;

  codec = avcodec_find_decoder((enum AVCodecID)opts.codec_id);
  if (!(codec_ctx = avcodec_alloc_context3(codec)))
    return AVERROR(ENOMEM);

  if ((errn = avcodec_parameters_to_context(codec_ctx, opts.params)) < 0)
    return errn;

  if ((errn = avcodec_open2(codec_ctx, codec, NULL)) < 0)
    return errn;

  codec_ctx->pkt_timebase = opts.timebase;

  if (!(ictx = (Decoder *)malloc(sizeof(Decoder))))
    return AVERROR(ENOMEM);

  ictx->codec_ctx = codec_ctx;
  if (!(ictx->output.ch_layout = malloc(sizeof(AVChannelLayout))))
    return AVERROR(ENOMEM);
  av_channel_layout_default(ictx->output.ch_layout, opts.output.nb_channels);

  ictx->output.sample_rate = opts.output.sample_rate;
  // Planar formats are not supported as they require a different procedure to
  // lay down the plain bits contained in their frames.
  ictx->output.sample_format =
      av_get_packed_sample_fmt(opts.output.sample_format);

  if ((errn = alloc_resampler(ictx)) < 0)
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
    if (!(resampled_frame = av_frame_alloc()))
      return AVERROR(ENOMEM);

    resampled_frame->nb_samples = frame->nb_samples;
    resampled_frame->ch_layout = *ctx->output.ch_layout;
    resampled_frame->sample_rate = ctx->output.sample_rate;
    resampled_frame->format = ctx->output.sample_format;

    if ((errn = av_frame_get_buffer(resampled_frame, 0)) != 0)
      return errn;

    if ((errn = swr_convert_frame(ctx->resampler_ctx, resampled_frame,
                                  frame)) != 0)
      return errn;

    resampled_frame->pts = next_pts;

    av_frame_unref(frame);
    av_frame_move_ref(frame, resampled_frame);
    av_frame_free(&resampled_frame);
  }

  return 0;
}

int decoder_free(Decoder **ctx) {
  avcodec_free_context(&(*ctx)->codec_ctx);

  if ((*ctx)->resampler_ctx) {
    swr_free(&(*ctx)->resampler_ctx);
    av_channel_layout_uninit((*ctx)->output.ch_layout);
  }

  free(*ctx);
  return 0;
}
