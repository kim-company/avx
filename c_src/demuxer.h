#include <libavformat/avformat.h>

typedef struct Demuxer Demuxer;

int demuxer_alloc_from_file(Demuxer **demuxer, char *path);
int demuxer_alloc_in_mem(Demuxer **demuxer, int probe_size);

// @doc Returns < 0 in case of error, 0 on success, > 1 to
// indicate the amount of data should be supplied (i.e. demand)
int demuxer_read_packet(Demuxer *ctx, AVPacket *packet);
int demuxer_add_data(Demuxer *ctx, void *data, int size);
int demuxer_read_streams(Demuxer *ctx, AVStream **streams, int *size);
int demuxer_demand(Demuxer *ctx);
int demuxer_is_ready(Demuxer *ctx);

void demuxer_free(Demuxer **ctx);
