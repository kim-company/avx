#include "libavcodec/packet.h"
#include "libavutil/frame.h"
#include "packet.h"

ErlNifResourceType *PACKET_RES_TYPE;
ErlNifResourceType *FRAME_RES_TYPE;

void free_packet_res(ErlNifEnv *env, void *res) {
  AVPacket **packet = (AVPacket **)res;
  av_packet_unref(*packet);
}

void get_packet(ErlNifEnv *env, ERL_NIF_TERM term, void **packet) {
  AVPacket **packet_res;
  enif_get_resource(env, term, PACKET_RES_TYPE, (void *)&packet_res);
  *packet = *packet_res;
}

void free_frame_res(ErlNifEnv *env, void *res) {
  AVFrame **frame = (AVFrame **)res;
  av_frame_unref(*frame);
}

void load_packet_resources(ErlNifEnv *env) {
  int flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;

  PACKET_RES_TYPE = enif_open_resource_type(env, NULL, "packet",
                                            free_packet_res, flags, NULL);
  FRAME_RES_TYPE =
      enif_open_resource_type(env, NULL, "frame", free_frame_res, flags, NULL);
}
