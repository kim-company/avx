# Environment variables passed via elixir_make
# ERTS_INCLUDE_DIR
# MIX_APP_PATH

# This works on rtx enabled machines. It is for direct usage only (dev).
ifndef ERTS_INCLUDE_DIR
ERTS_INCLUDE_DIR := $(shell rtx where erlang)/erts-14.0.2/include
endif
ifndef MIX_APP_PATH
MIX_APP_PATH := ..
endif

PRIV_DIR = $(MIX_APP_PATH)/priv
LIB_SO = $(PRIV_DIR)/libav.so
CC = clang
CFLAGS = -fPIC -g3 -shared -fno-omit-frame-pointer $(shell pkg-config --cflags libavcodec libavformat libavutil libswresample) -I$(ERTS_INCLUDE_DIR) -I.
LDFLAGS = -dynamiclib $(shell pkg-config --libs libavcodec libavformat libavutil libswresample)

ifeq ($(shell uname -s), Darwin)
LDFLAGS += -undefined dynamic_lookup
endif

all: $(LIB_SO)

$(LIB_SO): libav.c demuxer.c decoder.c
	@ mkdir -p $(PRIV_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(LIB_SO)

