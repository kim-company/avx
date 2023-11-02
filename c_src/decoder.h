#include "libavcodec/codec_par.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include <libavcodec/avcodec.h>

typedef struct {
  AVCodecContext *codec_ctx;
  SwrContext *resampler_ctx;

  struct {
    int sample_rate;
    enum AVSampleFormat sample_format;
    AVChannelLayout *ch_layout;
  } output;
} Decoder;

typedef struct {
  int codec_id;
  AVCodecParameters *params;
  AVRational timebase;

  struct {
    int sample_rate;
    int nb_channels;
    enum AVSampleFormat sample_format;
  } output;
} DecoderOpts;

int decoder_alloc(Decoder **ctx, DecoderOpts opts);
int decoder_send_packet(Decoder *ctx, AVPacket *packet);
int decoder_read_frame(Decoder *ctx, AVFrame *frame);
int decoder_free(Decoder **ctx);

void decoder_codec_ctx(Decoder *ctx, const AVCodecContext **codec_ctx);
