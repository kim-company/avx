#include "decoder.h"
#include "demuxer.h"
#include "packet.h"

// Called when the nif is loaded, as specified in the ERL_NIF_INIT call.
int load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info) {
  load_decoder_resources(env);
  load_demuxer_resources(env);
  load_packet_resources(env);

  return 0;
}

static ErlNifFunc nif_funcs[] = {
    // {erl_function_name, erl_function_arity, c_function}
    // Demuxer
    {"demuxer_alloc_context", 1, demuxer_alloc_context},
    {"demuxer_add_data", 2, demuxer_add_data},
    {"demuxer_is_ready", 1, demuxer_is_ready},
    {"demuxer_demand", 1, demuxer_demand},
    {"demuxer_streams", 1, demuxer_streams},
    {"demuxer_read_packet", 1, demuxer_read_packet},
    // Decoder
    {"decoder_alloc_context", 2, decoder_alloc_context},
    {"decoder_stream_format", 1, decoder_stream_format},
    {"decoder_add_data", 2, decoder_add_data}};

ERL_NIF_INIT(Elixir.AVx.NIF, nif_funcs, load, NULL, NULL, NULL)
