#include <libavformat/avformat.h>

typedef struct {
  AVFormatContext *fmt_ctx;
} Demuxer;

int demuxer_alloc_from_file(Demuxer **ctx, char *path);

// @doc Returns < 0 in case of error, 0 on success, > 1 to
// indicate the amount of data should be supplied (i.e. demand)
int demuxer_read_packet(Demuxer *ctx, AVPacket *packet);
void demuxer_free(Demuxer **ctx);
