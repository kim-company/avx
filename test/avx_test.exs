defmodule AVxTest do
  use ExUnit.Case

  alias AVx.{Demuxer, Decoder}

  @inputs [
    "test/data/safari.mp4",
    "/Users/dmorn/Downloads/multi-lang.mp4",
    "/Users/dmorn/projects/video-taxi-pepe-demo/test/data/babylon-30s-talk.mp4",
    "/Users/dmorn/projects/video-taxi-pepe-demo/test/data/babylon-30s-talk.mkv",
    "/Users/dmorn/projects/video-taxi-pepe-demo/test/data/babylon-30s-talk.ogg"
  ]

  for input <- @inputs do
    @tag :tmp_dir
    test "extract audio track from #{input}", %{tmp_dir: tmp_dir} do
      input = File.open!(unquote(input), [:raw, :read])
      output_path = Path.join([tmp_dir, "output.raw"])
      output = File.stream!(output_path, [:raw, :write])

      demuxer = Demuxer.new!(probe_size: 2048, input: input)

      read = fn input, size ->
        resp = IO.binread(input, size)
        {resp, input}
      end

      close = fn input -> File.close(input) end

      {streams, demuxer} = Demuxer.streams(demuxer, read)

      assert stream =
               %{codec_type: :audio} =
               Enum.find(streams, fn stream -> stream.codec_type == :audio end)

      packets =
        demuxer
        |> Demuxer.consume_packets([stream.stream_index], read, close)
        |> Stream.map(fn {_, packet} -> packet end)

      decoder = Decoder.new!(stream)

      assert %{channels: _, sample_rate: 48000, sample_format: "flt"} =
               Decoder.stream_format(decoder)

      decoder
      |> Decoder.decode_raw(packets)
      |> Enum.into(output)

      assert File.stat!(output_path).size > 0
    end
  end
end
