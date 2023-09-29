# AVx
Audio/Video Elixir. This is a libav (NIF) backed library for dealing with audio/video files. Takes
a functional approach allowing users to decide which runtime they want to design.

## Installation
I'll publish it at some point if there is interest. For now,

```elixir
def deps do
  [
    {:avx, github: "kim-company/avx"}
  ]
end
```

## Features
- [x] demux any audio/video container (mp4, ogg, mkv, ...)
- [x] decode audio files
- [ ] decode video files (in theory we should already be there, in practise the stream format for videos is not implemented and neither is the Frame "unpacking")

In the future it will support also encoding and muxing, in this order.

The term unpack is used here (probably incorrectly) to denote the action of
copying the packet/frame out of the C world into Elixir.

## Before you start
It will probably crash your BEAM at some point, as error handling is far from being complete on the C side. It
is already pretty stable though.

## Usage
Check the tests, but in practice this is the flow for decoding audio from a
multi-track file from file to file in a lazy fashion. Note that callers can
decide which type of reader implementation they want to provide.

```elixir
input = File.open!(input_path, [:raw, :read])
output = File.stream!(output_path, [:raw, :write])

demuxer = Demuxer.new!(probe_size: 2048, input: input)

read = fn input, size ->
  resp = IO.binread(input, size)
  {resp, input}
end

close = fn input -> File.close(input) end

# Detect available stream and select one (or more)
{streams, demuxer} = Demuxer.streams(demuxer, read)
audio_stream = Enum.find(streams, fn stream -> stream.codec_type == :audio end)
decoder = Decoder.new!(audio_stream)

packets =
  demuxer
  |> Demuxer.consume_packets([audio_stream.stream_index], read, close)
  |> Stream.map(fn {_, packet} -> packet end)

decoder
|> Decoder.decode_raw(packets)
|> Enum.into(output)
```

And that's it. Compared to using the `ffmpeg` executable directly, here you have access
to every single packet, which you can re-route, manipulate and process at will.

This library is suitable as standalone or inside the elements of a [membrane]
(https://github.com/membraneframework) pipeline for more complex setups (video
mixers with dynamic inputs, WebRTC rooms, ...)

## Debugging
This library works with NIFs. When things go wrong, the BEAM exits!
The idea is to run the `mix test` loop inside a debugger, in my case `lldb`.

To do so, we need to set some env variables, accomplished with `. DEBUG.fish` (check cocoa's link below if you shell is bash, or ask some LLM to translate the script 😉).
To run the tests under the debugger, `lldb -- $ERLEXEC $CMD_ARGS test`.

## Performance
Nothing serious in here for now, but to give an idea:
- M2 pro
- input file size 0.7Gb of ISO5 mp4
- output file size 1.3Gb of raw audio from 1 of the 3 tracks
- test **overall** duration 3.4 secs
- which gives 0.38Gb/s of output throughput

## Resources
- https://cocoa-research.works/2022/02/debug-erlang-nif-library/
- https://andrealeopardi.com/posts/using-c-from-elixir-with-nifs/
- https://www.erlang.org/doc/man/erl_nif
- https://lldb.llvm.org/use/map.html#breakpoint-commands
- https://github.com/membraneframework

## Copyright and License
Copyright 2023, [KIM Keep In Mind GmbH](https://www.keepinmind.info/)
Licensed under the [Apache License, Version 2.0](LICENSE)


