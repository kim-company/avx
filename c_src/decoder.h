#include "libavcodec/codec_par.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include <libavcodec/avcodec.h>

typedef struct {
  int sample_rate;
  int channels;
  enum AVSampleFormat sample_format;
} FormatOpts;

typedef struct {
  AVCodecContext *codec_ctx;
  SwrContext *resampler_ctx;

  FormatOpts *output_fmt;
} Decoder;

typedef struct {
  int codec_id;
  AVCodecParameters *params;
  AVRational timebase;

  FormatOpts output_opts;
} DecoderOpts;

int decoder_alloc(Decoder **ctx, DecoderOpts opts);
int decoder_send_packet(Decoder *ctx, AVPacket *packet);
int decoder_read_frame(Decoder *ctx, AVFrame *frame);
int decoder_free(Decoder **ctx);

void decoder_codec_ctx(Decoder *ctx, const AVCodecContext **codec_ctx);
