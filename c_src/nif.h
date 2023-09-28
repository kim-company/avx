#ifndef _NIF_H
#define _NIF_H

#include "erl_drv_nif.h"
#include <erl_nif.h>

void get_demuxer_ctx(ErlNifEnv *env, ERL_NIF_TERM term, void **ctx);
void free_demuxer_context_res(ErlNifEnv *env, void *res);

void get_decoder_ctx(ErlNifEnv *env, ERL_NIF_TERM term, void **ctx);
void free_decoder_context_res(ErlNifEnv *env, void *res);

void get_codec_params(ErlNifEnv *env, ERL_NIF_TERM term, void **params);
void free_codec_params_res(ErlNifEnv *env, void *res);

void get_packet(ErlNifEnv *env, ERL_NIF_TERM term, void **packet);
void free_packet_res(ErlNifEnv *env, void *res);

#endif
