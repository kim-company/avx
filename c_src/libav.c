#include "libavutil/log.h"
#include "libswresample/swresample.h"
#include <demuxer.h>
#include <erl_nif.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>

#define DEFAULT_PROBE_SIZE 1024 * 2

ErlNifResourceType *DEMUXER_RES_TYPE;
ErlNifResourceType *CODEC_PARAMS_RES_TYPE;
ErlNifResourceType *DECODER_CTX_RES_TYPE;
ErlNifResourceType *PACKET_RES_TYPE;
ErlNifResourceType *FRAME_RES_TYPE;

void free_demuxer_res(ErlNifEnv *env, void *res) {
  Demuxer **ctx = (Demuxer **)res;
  demuxer_free(ctx);
}

void free_codec_params_res(ErlNifEnv *env, void *res) {
  AVCodecParameters **params = (AVCodecParameters **)res;
  avcodec_parameters_free(params);
}

void get_codec_params(ErlNifEnv *env, ERL_NIF_TERM term,
                      AVCodecParameters **ctx) {
  AVCodecParameters **params_res;
  enif_get_resource(env, term, CODEC_PARAMS_RES_TYPE, (void *)&params_res);
  *ctx = *params_res;
}

void enif_get_demuxer(ErlNifEnv *env, ERL_NIF_TERM term, Demuxer **ctx) {
  Demuxer **ctx_res;
  enif_get_resource(env, term, DEMUXER_RES_TYPE, (void *)&ctx_res);
  *ctx = *ctx_res;
}

ERL_NIF_TERM enif_make_error(ErlNifEnv *env, char *err) {
  return enif_make_tuple2(env, enif_make_atom(env, "error"),
                          enif_make_string(env, err, ERL_NIF_UTF8));
}

ERL_NIF_TERM enif_make_av_error(ErlNifEnv *env, int errn) {
  char err[128];
  av_strerror(errn, err, sizeof(err));
  return enif_make_error(env, err);
}

ERL_NIF_TERM enif_make_demuxer_res(ErlNifEnv *env, Demuxer *demuxer) {
  // Make the resource take ownership on the context.
  Demuxer **ctx_res = enif_alloc_resource(DEMUXER_RES_TYPE, sizeof(Demuxer *));
  *ctx_res = demuxer;

  ERL_NIF_TERM term = enif_make_resource(env, ctx_res);

  // This is done to allow the erlang garbage collector to take care
  // of freeing this resource when needed.
  enif_release_resource(ctx_res);

  return term;
}

ERL_NIF_TERM enif_demuxer_alloc_from_file(ErlNifEnv *env, int argc,
                                          const ERL_NIF_TERM argv[]) {
  Demuxer *ctx;
  ErlNifBinary binary;
  char *path;
  int errn;

  enif_inspect_binary(env, argv[0], &binary);
  path = (char *)malloc(binary.size);
  memcpy(path, binary.data, binary.size);

  errn = demuxer_alloc_from_file(&ctx, path);
  free(path);

  if (errn < 0)
    return enif_make_av_error(env, errn);
  return enif_make_demuxer_res(env, ctx);
}

ERL_NIF_TERM enif_demuxer_alloc_in_mem(ErlNifEnv *env, int argc,
                                       const ERL_NIF_TERM argv[]) {
  int probe_size, errn;
  Demuxer *ctx;

  enif_get_int(env, argv[0], &probe_size);
  if (probe_size <= 0)
    probe_size = DEFAULT_PROBE_SIZE;

  if ((errn = demuxer_alloc_in_mem(&ctx, probe_size)) < 0)
    return enif_make_av_error(env, errn);

  return enif_make_demuxer_res(env, ctx);
}

ERL_NIF_TERM enif_demuxer_add_data(ErlNifEnv *env, int argc,
                                   const ERL_NIF_TERM argv[]) {
  Demuxer *ctx;
  ErlNifBinary binary;
  int errn;

  enif_get_demuxer(env, argv[0], &ctx);
  enif_inspect_binary(env, argv[1], &binary);

  if ((errn = demuxer_add_data(ctx, binary.data, binary.size)) < 0)
    return enif_make_error(env, "add data not allowed");

  return enif_make_atom(env, "ok");
}

ERL_NIF_TERM enif_demuxer_is_ready(ErlNifEnv *env, int argc,
                                   const ERL_NIF_TERM argv[]) {
  Demuxer *ctx;
  enif_get_demuxer(env, argv[0], &ctx);

  return enif_make_int(env, demuxer_is_ready(ctx));
}

ERL_NIF_TERM enif_demuxer_read_packet(ErlNifEnv *env, int argc,
                                      const ERL_NIF_TERM argv[]) {
  Demuxer *ctx;
  AVPacket *packet;
  int ret;

  packet = av_packet_alloc();

  enif_get_demuxer(env, argv[0], &ctx);

  if ((ret = demuxer_read_packet(ctx, packet)) != 0) {
    if (ret == AVERROR_EOF)
      return enif_make_atom(env, "eof");
    if (ret < 0)
      return enif_make_av_error(env, ret);
    if (ret > 0)
      return enif_make_tuple2(env, enif_make_atom(env, "demand"),
                              enif_make_long(env, ret));
  }

  // Make the resource
  AVPacket **res = enif_alloc_resource(PACKET_RES_TYPE, sizeof(AVPacket *));
  *res = packet;

  ERL_NIF_TERM term = enif_make_resource(env, res);

  // This is done to allow the erlang garbage collector to take care
  // of freeing this resource when needed.
  enif_release_resource(res);

  return enif_make_tuple2(env, enif_make_atom(env, "ok"), term);
}

ERL_NIF_TERM enif_make_stream_map(ErlNifEnv *env, AVStream *stream) {
  ERL_NIF_TERM codec_term;
  ErlNifBinary *binary;
  AVCodecParameters *params = NULL;
  const char *codec_name;

  // Parameters are used to preserve as much information
  // as possible when creating a new Codec. We're making
  // a copy to ensure we own this data.
  // TODO: DO WE?

  params = avcodec_parameters_alloc();
  avcodec_parameters_copy(params, stream->codecpar);

  // Make the resource
  AVCodecParameters **codec_params_res =
      enif_alloc_resource(CODEC_PARAMS_RES_TYPE, sizeof(AVCodecParameters *));
  *codec_params_res = params;

  ERL_NIF_TERM res_term = enif_make_resource(env, codec_params_res);

  // This is done to allow the erlang garbage collector to take care
  // of freeing this resource when needed.
  enif_release_resource(codec_params_res);

  // Create the returned map of information.
  codec_name = avcodec_get_name(params->codec_id);

  ERL_NIF_TERM map;
  map = enif_make_new_map(env);

  ERL_NIF_TERM codec_type;
  switch (params->codec_type) {
  case AVMEDIA_TYPE_AUDIO:
    codec_type = enif_make_atom(env, "audio");
    break;
  case AVMEDIA_TYPE_VIDEO:
    codec_type = enif_make_atom(env, "video");
    break;
  default:
    codec_type = enif_make_atom(env, "und");
    break;
  }

  enif_make_map_put(env, map, enif_make_atom(env, "codec_id"),
                    enif_make_int(env, params->codec_id), &map);
  enif_make_map_put(env, map, enif_make_atom(env, "codec_type"), codec_type,
                    &map);
  enif_make_map_put(env, map, enif_make_atom(env, "codec_name"),
                    enif_make_string(env, codec_name, ERL_NIF_UTF8), &map);
  enif_make_map_put(env, map, enif_make_atom(env, "codec_params"), res_term,
                    &map);
  enif_make_map_put(env, map, enif_make_atom(env, "stream_index"),
                    enif_make_int(env, stream->index), &map);
  enif_make_map_put(env, map, enif_make_atom(env, "timebase_num"),
                    enif_make_int(env, stream->time_base.num), &map);
  enif_make_map_put(env, map, enif_make_atom(env, "timebase_den"),
                    enif_make_int(env, stream->time_base.den), &map);

  // TODO
  // Expand the information available to Elixir: here we can only select a
  // stream by index and codec, quality or language should be available too.
  return map;
}

ERL_NIF_TERM enif_demuxer_read_streams(ErlNifEnv *env, int argc,
                                       const ERL_NIF_TERM argv[]) {
  ERL_NIF_TERM *codecs;
  Demuxer *ctx;
  AVStream *streams;
  unsigned int size;
  int errn;

  enif_get_demuxer(env, argv[0], &ctx);

  if ((errn = demuxer_read_streams(ctx, &streams, &size)) < 0)
    return enif_make_av_error(env, errn);

  codecs = calloc(size, sizeof(ERL_NIF_TERM));
  for (int i = 0; i < size; i++) {
    codecs[i] = enif_make_stream_map(env, &streams[i]);
  }

  return enif_make_tuple2(env, enif_make_atom(env, "ok"),
                          enif_make_list_from_array(env, codecs, size));
}

int enif_get_packet(ErlNifEnv *env, ERL_NIF_TERM term, AVPacket **packet) {
  AVPacket **packet_res;
  int ret;

  ret = enif_get_resource(env, term, PACKET_RES_TYPE, (void *)&packet_res);
  *packet = *packet_res;

  return ret;
}

ERL_NIF_TERM enif_packet_unpack(ErlNifEnv *env, int argc,
                                const ERL_NIF_TERM argv[]) {
  AVPacket *packet;
  ERL_NIF_TERM map, data;

  enif_get_packet(env, argv[0], &packet);

  void *ptr = enif_make_new_binary(env, packet->size, &data);
  memcpy(ptr, packet->data, packet->size);

  map = enif_make_new_map(env);
  enif_make_map_put(env, map, enif_make_atom(env, "pts"),
                    enif_make_long(env, packet->pts), &map);
  enif_make_map_put(env, map, enif_make_atom(env, "dts"),
                    enif_make_long(env, packet->dts), &map);
  enif_make_map_put(env, map, enif_make_atom(env, "data"), data, &map);

  return map;
}

ERL_NIF_TERM enif_demuxer_demand(ErlNifEnv *env, int argc,
                                 const ERL_NIF_TERM argv[]) {
  Demuxer *ctx;
  enif_get_demuxer(env, argv[0], &ctx);

  return enif_make_int(env, demuxer_demand(ctx));
}

typedef struct {
  AVCodecContext *codec_ctx;
  SwrContext *resampler_ctx;
  enum AVSampleFormat output_sample_format;
} DecoderCtx;

void free_decoder_context_res(ErlNifEnv *env, void *res) {
  DecoderCtx **ctx = (DecoderCtx **)res;
  avcodec_free_context(&(*ctx)->codec_ctx);
  if ((*ctx)->resampler_ctx)
    swr_free(&(*ctx)->resampler_ctx);

  free(*ctx);
}

void get_decoder_context(ErlNifEnv *env, ERL_NIF_TERM term, DecoderCtx **ctx) {
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

  // Intermediate decoding data.
  ERL_NIF_TERM tmp;
  int num, den;

  enif_get_map_value(env, argv[0], enif_make_atom(env, "codec_id"), &tmp);
  enif_get_int(env, tmp, &codec_id);

  enif_get_map_value(env, argv[0], enif_make_atom(env, "codec_params"), &tmp);
  get_codec_params(env, tmp, &params);

  enif_get_map_value(env, argv[0], enif_make_atom(env, "timebase_num"), &tmp);
  enif_get_int(env, tmp, &num);

  enif_get_map_value(env, argv[0], enif_make_atom(env, "timebase_den"), &tmp);
  enif_get_int(env, tmp, &den);

  codec = avcodec_find_decoder((enum AVCodecID)codec_id);
  codec_ctx = avcodec_alloc_context3(codec);

  avcodec_parameters_to_context(codec_ctx, params);
  codec_ctx->pkt_timebase.num = num;
  codec_ctx->pkt_timebase.den = den;

  avcodec_open2(codec_ctx, codec, NULL);

  ctx = (DecoderCtx *)malloc(sizeof(DecoderCtx));
  ctx->codec_ctx = codec_ctx;
  ctx->output_sample_format = codec_ctx->sample_fmt;

  // Resampler is here to play well with the Membrane framework, which
  // does not support planar PCM.
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
  get_decoder_context(env, argv[0], &ctx);

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

ERL_NIF_TERM packet_stream_index(ErlNifEnv *env, int argc,
                                 const ERL_NIF_TERM argv[]) {
  AVPacket *packet;
  enif_get_packet(env, argv[0], &packet);
  return enif_make_int(env, packet->stream_index);
}

ERL_NIF_TERM decoder_add_data(ErlNifEnv *env, int argc,
                              const ERL_NIF_TERM argv[]) {
  DecoderCtx *ctx;
  AVPacket *packet;
  AVFrame *frame;
  ErlNifBinary binary;
  ERL_NIF_TERM map_value;
  ERL_NIF_TERM list;
  char err[256];
  int ret;

  get_decoder_context(env, argv[0], &ctx);
  if ((ret = enif_get_packet(env, argv[1], &packet))) {
    avcodec_send_packet(ctx->codec_ctx, packet);
  } else {
    // This is a "drain" packet, i.e. NULL.
    avcodec_send_packet(ctx->codec_ctx, NULL);
  }

  list = enif_make_list(env, 0);
  frame = av_frame_alloc();
  while ((ret = avcodec_receive_frame(ctx->codec_ctx, frame)) == 0) {
    enum AVSampleFormat actual_format = frame->format;
    AVFrame *oframe;

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

    oframe = av_frame_alloc();
    av_frame_ref(oframe, frame);

    // Make the resource take ownership on the context.
    AVFrame **frame_res =
        enif_alloc_resource(FRAME_RES_TYPE, sizeof(AVFrame *));
    *frame_res = oframe;

    ERL_NIF_TERM term = enif_make_resource(env, frame_res);

    // This is done to allow the erlang garbage collector to take care
    // of freeing this resource when needed.
    enif_release_resource(frame_res);
    list = enif_make_list_cell(env, term, list);

    // Reset the frame to reuse it for the next decode round.
    av_frame_unref(frame);
  }
  av_frame_unref(frame);

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

int get_frame(ErlNifEnv *env, ERL_NIF_TERM term, AVFrame **frame) {
  AVFrame **frame_res;
  int ret;

  ret = enif_get_resource(env, term, FRAME_RES_TYPE, (void *)&frame_res);
  *frame = *frame_res;

  return ret;
}

ERL_NIF_TERM frame_unpack(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  AVFrame *frame;
  ERL_NIF_TERM list;

  get_frame(env, argv[0], &frame);

  list = enif_make_list(env, 0);
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

  return enif_make_tuple2(env, enif_make_atom(env, "ok"), list);
}

void free_packet_res(ErlNifEnv *env, void *res) {
  AVPacket **packet = (AVPacket **)res;
  av_packet_unref(*packet);
}

void free_frame_res(ErlNifEnv *env, void *res) {
  AVFrame **frame = (AVFrame **)res;
  av_frame_unref(*frame);
}

// Called when the nif is loaded, as specified in the ERL_NIF_INIT call.
int load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info) {
  av_log_set_level(AV_LOG_QUIET);

  int flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;
  DEMUXER_RES_TYPE = enif_open_resource_type(env, NULL, "demuxer_ctx",
                                             free_demuxer_res, flags, NULL);

  CODEC_PARAMS_RES_TYPE = enif_open_resource_type(
      env, NULL, "codec_params", free_codec_params_res, flags, NULL);

  DECODER_CTX_RES_TYPE = enif_open_resource_type(
      env, NULL, "decoder_ctx", free_decoder_context_res, flags, NULL);

  PACKET_RES_TYPE = enif_open_resource_type(env, NULL, "packet",
                                            free_packet_res, flags, NULL);

  FRAME_RES_TYPE =
      enif_open_resource_type(env, NULL, "frame", free_frame_res, flags, NULL);

  return 0;
}

static ErlNifFunc nif_funcs[] = {
    // TODO
    // Some of these functions are IO dirty.

    // {erl_function_name, erl_function_arity, c_function}
    // Demuxer
    {"demuxer_alloc_from_file", 1, enif_demuxer_alloc_from_file},
    {"demuxer_alloc", 1, enif_demuxer_alloc_in_mem},
    {"demuxer_streams", 1, enif_demuxer_read_streams},
    {"demuxer_read_packet", 1, enif_demuxer_read_packet},
    {"demuxer_add_data", 2, enif_demuxer_add_data},
    {"demuxer_is_ready", 1, enif_demuxer_is_ready},
    {"demuxer_demand", 1, enif_demuxer_demand},
    // Decoder
    {"decoder_alloc_context", 1, decoder_alloc_context},
    {"decoder_stream_format", 1, decoder_stream_format},
    {"decoder_add_data", 2, decoder_add_data},
    // General
    {"packet_stream_index", 1, packet_stream_index},
    {"packet_unpack", 1, enif_packet_unpack},
    {"frame_unpack", 1, frame_unpack},
};

ERL_NIF_INIT(Elixir.AVx.NIF, nif_funcs, load, NULL, NULL, NULL)
