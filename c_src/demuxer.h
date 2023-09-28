#ifndef _DEMUXER_H
#define _DEMUXER_H

#include "nif.h"

typedef enum { CTX_MODE_DRAIN, CTX_MODE_BUF } CTX_MODE;
typedef struct DemuxerCtx DemuxerCtx;

void load_demuxer_resources(ErlNifEnv *env);

ERL_NIF_TERM demuxer_alloc_context(ErlNifEnv *env, int argc,
                                   const ERL_NIF_TERM argv[]);
ERL_NIF_TERM demuxer_add_data(ErlNifEnv *env, int argc,
                              const ERL_NIF_TERM argv[]);
ERL_NIF_TERM demuxer_is_ready(ErlNifEnv *env, int argc,
                              const ERL_NIF_TERM argv[]);
ERL_NIF_TERM demuxer_read_packet(ErlNifEnv *env, int argc,
                                 const ERL_NIF_TERM argv[]);
ERL_NIF_TERM demuxer_streams(ErlNifEnv *env, int argc,
                             const ERL_NIF_TERM argv[]);
ERL_NIF_TERM demuxer_demand(ErlNifEnv *env, int argc,
                            const ERL_NIF_TERM argv[]);
#endif
