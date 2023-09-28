#ifndef _DECODER_H
#define _DECODER_H

#include "nif.h"

typedef struct DecoderCtx DecoderCtx;

void load_decoder_resources(ErlNifEnv *env);

ERL_NIF_TERM decoder_alloc_context(ErlNifEnv *env, int argc,
                                   const ERL_NIF_TERM argv[]);
ERL_NIF_TERM decoder_stream_format(ErlNifEnv *env, int argc,
                                   const ERL_NIF_TERM argv[]);
ERL_NIF_TERM decoder_add_data(ErlNifEnv *env, int argc,
                              const ERL_NIF_TERM argv[]);

#endif
