#include "decoder.h"
#include "libswresample/swresample.h"
#include <libavcodec/avcodec.h>

ErlNifResourceType *DECODER_CTX_RES_TYPE;

struct DecoderCtx {
  AVCodecContext *codec_ctx;
  SwrContext *resampler_ctx;
  enum AVSampleFormat output_sample_format;
};

void load_decoder_resources(ErlNifEnv *env) {
  int flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;
  DECODER_CTX_RES_TYPE = enif_open_resource_type(
      env, NULL, "decoder_ctx", free_decoder_context_res, flags, NULL);
}

void free_decoder_context_res(ErlNifEnv *env, void *res) {
  DecoderCtx **ctx = (DecoderCtx **)res;
  avcodec_free_context(&(*ctx)->codec_ctx);
  if ((*ctx)->resampler_ctx)
    swr_free(&(*ctx)->resampler_ctx);

  free(*ctx);
}

void get_decoder_ctx(ErlNifEnv *env, ERL_NIF_TERM term, void **ctx) {
  DecoderCtx **ctx_res;
  enif_get_resource(env, term, DECODER_CTX_RES_TYPE, (void *)&ctx_res);
  *ctx = *ctx_res;
}

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

int alloc_resampler(DecoderCtx *ctx) {
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

ERL_NIF_TERM decoder_alloc_context(ErlNifEnv *env, int argc,
                                   const ERL_NIF_TERM argv[]) {
  int codec_id;
  AVCodecParameters *params;
  const AVCodec *codec;
  AVCodecContext *codec_ctx;
  DecoderCtx *ctx;

  enif_get_int(env, argv[0], &codec_id);
  get_codec_params(env, argv[1], (void *)&params);

  codec = avcodec_find_decoder((enum AVCodecID)codec_id);
  codec_ctx = avcodec_alloc_context3(codec);

  avcodec_parameters_to_context(codec_ctx, params);
  avcodec_open2(codec_ctx, codec, NULL);

  ctx = (DecoderCtx *)malloc(sizeof(DecoderCtx));
  ctx->codec_ctx = codec_ctx;
  ctx->output_sample_format = codec_ctx->sample_fmt;

  if (is_planar(codec_ctx->sample_fmt))
    alloc_resampler(ctx);

  // Make the resource take ownership on the context.
  DecoderCtx **ctx_res =
      enif_alloc_resource(DECODER_CTX_RES_TYPE, sizeof(DecoderCtx *));
  *ctx_res = ctx;

  ERL_NIF_TERM term = enif_make_resource(env, ctx_res);

  // This is done to allow the erlang garbage collector to take care
  // of freeing this resource when needed.
  enif_release_resource(ctx_res);

  return term;
}

ERL_NIF_TERM decoder_stream_format(ErlNifEnv *env, int argc,
                                   const ERL_NIF_TERM argv[]) {
  DecoderCtx *ctx;
  enum AVSampleFormat fmt;
  ERL_NIF_TERM map;
  get_decoder_ctx(env, argv[0], (void *)&ctx);

  // TODO
  // this function is only meaningful when the stream is of audio type. To
  // support video, add another if condition or a switch and add the relevant
  // information to the map.

  map = enif_make_new_map(env);

  if (ctx->codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {

    enif_make_map_put(env, map, enif_make_atom(env, "channels"),
                      enif_make_int(env, ctx->codec_ctx->ch_layout.nb_channels),
                      &map);
    enif_make_map_put(env, map, enif_make_atom(env, "sample_rate"),
                      enif_make_int(env, ctx->codec_ctx->sample_rate), &map);
    enif_make_map_put(
        env, map, enif_make_atom(env, "sample_format"),
        enif_make_string(env, av_get_sample_fmt_name(ctx->output_sample_format),
                         ERL_NIF_UTF8),
        &map);
  }

  return map;
}

ERL_NIF_TERM decoder_add_data(ErlNifEnv *env, int argc,
                              const ERL_NIF_TERM argv[]) {
  DecoderCtx *ctx;
  AVPacket *ipacket, *packet;
  AVFrame *frame;
  ErlNifBinary binary;
  ERL_NIF_TERM map_value;
  ERL_NIF_TERM list;
  char err[256];
  int ret;

  get_decoder_ctx(env, argv[0], (void *)&ctx);

  // decode the fields of the input map.
  if ((ret = enif_get_map_value(env, argv[1], enif_make_atom(env, "data"),
                                &map_value))) {
    ipacket = av_packet_alloc();
    enif_inspect_binary(env, map_value, &binary);
    ipacket->data = binary.data;
    ipacket->size = binary.size;

    enif_get_map_value(env, argv[1], enif_make_atom(env, "pts"), &map_value);
    enif_get_int64(env, map_value, (long *)&ipacket->pts);

    enif_get_map_value(env, argv[1], enif_make_atom(env, "dts"), &map_value);
    enif_get_int64(env, map_value, (long *)&ipacket->dts);

    packet = av_packet_alloc();
    av_packet_ref(packet, ipacket);
    av_packet_free(&ipacket);

    avcodec_send_packet(ctx->codec_ctx, packet);
    av_packet_unref(packet);
  } else {
    // This is a "drain" packet, i.e. NULL.
    avcodec_send_packet(ctx->codec_ctx, NULL);
  };

  list = enif_make_list(env, 0);
  frame = av_frame_alloc();
  while ((ret = avcodec_receive_frame(ctx->codec_ctx, frame)) == 0) {
    enum AVSampleFormat actual_format = frame->format;

    if (ctx->resampler_ctx) {
      AVFrame *resampled_frame;
      resampled_frame = av_frame_alloc();
      resampled_frame->nb_samples = frame->nb_samples;
      resampled_frame->ch_layout = frame->ch_layout;
      resampled_frame->sample_rate = frame->sample_rate;
      resampled_frame->format = ctx->output_sample_format;

      av_frame_get_buffer(resampled_frame, 0);

      swr_convert_frame(ctx->resampler_ctx, resampled_frame, frame);
      resampled_frame->pts = frame->pts;

      av_frame_unref(frame);
      frame = resampled_frame;
    }

    AVBufferRef *ref;
    for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
      if (!(ref = frame->buf[i]))
        break;

      ERL_NIF_TERM data;
      ERL_NIF_TERM map;

      void *ptr = enif_make_new_binary(env, ref->size, &data);
      memcpy(ptr, ref->data, ref->size);

      // Create a frame map with the data contained in each
      // buffer reference.
      map = enif_make_new_map(env);
      // TODO
      // each frame map created here will have the same pts value.
      // To solve, we might divide frame->duration with the number of
      // buffers found duration this process.
      enif_make_map_put(env, map, enif_make_atom(env, "pts"),
                        enif_make_long(env, frame->pts), &map);
      enif_make_map_put(env, map, enif_make_atom(env, "data"), data, &map);

      list = enif_make_list_cell(env, map, list);
    }
    // Reset the frame to reuse it for the next decode round.
    av_frame_unref(frame);
  }

  switch (ret) {
  case 0:
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), list);
  case AVERROR(EAGAIN):
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), list);
  case AVERROR_EOF:
    return enif_make_tuple2(env, enif_make_atom(env, "eof"), list);
  default:
    av_strerror(ret, err, sizeof(err));
    return enif_make_tuple2(env, enif_make_atom(env, "error"),
                            enif_make_string(env, err, ERL_NIF_UTF8));
  }
}
