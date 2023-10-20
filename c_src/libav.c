#include "libavcodec/codec_par.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include <decoder.h>
#include <demuxer.h>
#include <erl_nif.h>

#define DEFAULT_PROBE_SIZE 1024 * 2

ErlNifResourceType *DEMUXER_RES_TYPE;
ErlNifResourceType *CODEC_PARAMS_RES_TYPE;
ErlNifResourceType *DECODER_RES_TYPE;
ErlNifResourceType *PACKET_RES_TYPE;
ErlNifResourceType *FRAME_RES_TYPE;

void enif_free_demuxer(ErlNifEnv *env, void *res) {
  Demuxer **ctx = (Demuxer **)res;
  demuxer_free(ctx);
}

void enif_free_codec_params(ErlNifEnv *env, void *res) {
  AVCodecParameters **params = (AVCodecParameters **)res;
  avcodec_parameters_free(params);
}

void enif_get_codec_params(ErlNifEnv *env, ERL_NIF_TERM term,
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
  // as possible when creating a new Codec.
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
  AVFormatContext *fmt_ctx;

  enif_get_demuxer(env, argv[0], &ctx);
  demuxer_fmt_ctx(ctx, &fmt_ctx);

  codecs = calloc(fmt_ctx->nb_streams, sizeof(ERL_NIF_TERM));
  for (int i = 0; i < fmt_ctx->nb_streams; i++) {
    codecs[i] = enif_make_stream_map(env, fmt_ctx->streams[i]);
  }

  return enif_make_tuple2(
      env, enif_make_atom(env, "ok"),
      enif_make_list_from_array(env, codecs, fmt_ctx->nb_streams));
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

void enif_free_decoder(ErlNifEnv *env, void *res) {
  Decoder **ctx = (Decoder **)res;
  decoder_free(ctx);
}

void enif_get_decoder(ErlNifEnv *env, ERL_NIF_TERM term, Decoder **ctx) {
  Decoder **ctx_res;
  enif_get_resource(env, term, DECODER_RES_TYPE, (void *)&ctx_res);
  *ctx = *ctx_res;
}

ERL_NIF_TERM enif_decoder_alloc(ErlNifEnv *env, int argc,
                                const ERL_NIF_TERM argv[]) {
  int codec_id;
  AVCodecParameters *params;
  Decoder *ctx;
  int errn;

  // Intermediate decoding data.
  ERL_NIF_TERM tmp;

  enif_get_map_value(env, argv[0], enif_make_atom(env, "codec_id"), &tmp);
  enif_get_int(env, tmp, &codec_id);

  enif_get_map_value(env, argv[0], enif_make_atom(env, "codec_params"), &tmp);
  enif_get_codec_params(env, tmp, &params);

  if ((errn = decoder_alloc(&ctx, (enum AVCodecID)codec_id, params)) < 0)
    return enif_make_av_error(env, errn);

  // Make the resource take ownership on the context.
  Decoder **ctx_res = enif_alloc_resource(DECODER_RES_TYPE, sizeof(Decoder *));
  *ctx_res = ctx;

  ERL_NIF_TERM term = enif_make_resource(env, ctx_res);

  // This is done to allow the erlang garbage collector to take care
  // of freeing this resource when needed.
  enif_release_resource(ctx_res);

  return term;
}

ERL_NIF_TERM enif_decoder_stream_format(ErlNifEnv *env, int argc,
                                        const ERL_NIF_TERM argv[]) {
  Decoder *ctx;
  ERL_NIF_TERM map;

  enif_get_decoder(env, argv[0], &ctx);

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

ERL_NIF_TERM enif_packet_stream_index(ErlNifEnv *env, int argc,
                                      const ERL_NIF_TERM argv[]) {
  AVPacket *packet;
  enif_get_packet(env, argv[0], &packet);
  return enif_make_int(env, packet->stream_index);
}

ERL_NIF_TERM enif_decoder_add_data(ErlNifEnv *env, int argc,
                                   const ERL_NIF_TERM argv[]) {
  Decoder *ctx;
  AVPacket *packet;
  AVFrame *frame;
  ErlNifBinary binary;
  ERL_NIF_TERM list;
  char err[256];
  int errn;

  enif_get_decoder(env, argv[0], &ctx);

  if (!enif_get_packet(env, argv[1], &packet))
    // This is a "drain" packet, i.e. NULL.
    packet = NULL;

  // NOTE
  // This function could also send an EGAIN error, which is not a terminating
  // error: it just means that we have to read before we're allowed to send this
  // packet again. In this loop it should not happen though.
  if ((errn = decoder_send_packet(ctx, packet)) != 0)
    return enif_make_av_error(env, errn);

  list = enif_make_list(env, 0);
  frame = av_frame_alloc();
  while ((errn = decoder_read_frame(ctx, frame)) == 0) {
    AVFrame *oframe;
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

  switch (errn) {
  case 0:
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), list);
  case AVERROR(EAGAIN):
    return enif_make_tuple2(env, enif_make_atom(env, "ok"), list);
  case AVERROR_EOF:
    return enif_make_tuple2(env, enif_make_atom(env, "eof"), list);
  default:
    return enif_make_av_error(env, errn);
  }
}

int enif_get_frame(ErlNifEnv *env, ERL_NIF_TERM term, AVFrame **frame) {
  AVFrame **frame_res;
  int ret;

  ret = enif_get_resource(env, term, FRAME_RES_TYPE, (void *)&frame_res);
  *frame = *frame_res;

  return ret;
}

ERL_NIF_TERM enif_frame_unpack(ErlNifEnv *env, int argc,
                               const ERL_NIF_TERM argv[]) {
  AVFrame *frame;
  ERL_NIF_TERM list;

  enif_get_frame(env, argv[0], &frame);

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

void enif_free_packet(ErlNifEnv *env, void *res) {
  AVPacket **packet = (AVPacket **)res;
  av_packet_unref(*packet);
}

void enif_free_frame(ErlNifEnv *env, void *res) {
  AVFrame **frame = (AVFrame **)res;
  av_frame_unref(*frame);
}

// Called when the nif is loaded, as specified in the ERL_NIF_INIT call.
int load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info) {
  av_log_set_level(AV_LOG_QUIET);

  int flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;
  DEMUXER_RES_TYPE = enif_open_resource_type(env, NULL, "demuxer",
                                             enif_free_demuxer, flags, NULL);

  CODEC_PARAMS_RES_TYPE = enif_open_resource_type(
      env, NULL, "codec_params", enif_free_codec_params, flags, NULL);

  DECODER_RES_TYPE = enif_open_resource_type(env, NULL, "decoder",
                                             enif_free_decoder, flags, NULL);

  PACKET_RES_TYPE = enif_open_resource_type(env, NULL, "packet",
                                            enif_free_packet, flags, NULL);

  FRAME_RES_TYPE =
      enif_open_resource_type(env, NULL, "frame", enif_free_frame, flags, NULL);

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
    {"decoder_alloc", 1, enif_decoder_alloc},
    {"decoder_stream_format", 1, enif_decoder_stream_format},
    {"decoder_add_data", 2, enif_decoder_add_data},
    // General
    {"packet_stream_index", 1, enif_packet_stream_index},
    {"packet_unpack", 1, enif_packet_unpack},
    {"frame_unpack", 1, enif_frame_unpack},
};

ERL_NIF_INIT(Elixir.AVx.NIF, nif_funcs, load, NULL, NULL, NULL)
