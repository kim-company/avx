#include "libswresample/swresample.h"
#include <libavcodec/avcodec.h>

typedef struct Decoder Decoder;
struct Decoder {
  AVCodecContext *codec_ctx;
  SwrContext *resampler_ctx;
  enum AVSampleFormat output_sample_format;
};

int decoder_alloc(Decoder **ctx, enum AVCodecID codec_id,
                  AVCodecParameters *params, AVRational timebase);
int decoder_send_packet(Decoder *ctx, AVPacket *packet);
int decoder_read_frame(Decoder *ctx, AVFrame *frame);
int decoder_free(Decoder **ctx);

void decoder_codec_ctx(Decoder *ctx, const AVCodecContext **codec_ctx);
