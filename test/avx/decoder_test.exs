defmodule AVx.DecoderTest do
  use ExUnit.Case

  alias AVx.{Demuxer, Decoder, Frame}

  @inputs [
    "test/data/mic.mp4",
    "test/data/mic.mp3",
    "test/data/mic.ogg",
    "test/data/mic.mkv",
    "test/data/mic.aac",
    "test/data/packed.aac"
  ]

  for input <- @inputs do
    test "extract audio track from #{input}" do
      demuxer = Demuxer.new_from_file(unquote(input))
      {streams, demuxer} = Demuxer.streams(demuxer)

      assert stream =
               %{codec_type: :audio} =
               Enum.find(streams, fn stream -> stream.codec_type == :audio end)

      packets =
        demuxer
        |> Demuxer.consume_packets([stream.stream_index])
        |> Stream.map(fn {_, packet} -> packet end)

      decoder = Decoder.new!(stream)

      assert %{channels: _, sample_rate: 48000, sample_format: "flt"} =
               Decoder.stream_format(decoder)

      info = Support.show_frames(unquote(input))
      assert_frames(packets, decoder, info)
    end
  end

  defp assert_frames(packets, decoder, expected_frames) do
    count =
      decoder
      |> Decoder.decode_frames(packets)
      |> Stream.flat_map(&Frame.unpack(&1))
      |> Stream.zip(expected_frames)
      |> Enum.reduce(0, fn {have, want}, count ->
        assert have.pts == Map.fetch!(want, "pts")
        count + 1
      end)

    assert count == length(expected_frames)
  end
end
