defmodule AVx.DemuxerTest do
  use ExUnit.Case

  alias AVx.{Demuxer, Packet}

  @input "test/data/mic.mp4"

  describe "demuxer" do
    test "from file" do
      {:ok, demuxer} = Demuxer.new_from_file(@input)
      streams = Demuxer.read_streams(demuxer)

      stream = Enum.find(streams, fn stream -> stream.codec_type == :audio end)

      info = Support.show_packets(@input)
      assert_packets(demuxer, [stream.stream_index], info)
    end

    defp assert_packets(demuxer, indexes, expected_packets) do
      count =
        demuxer
        |> Demuxer.consume_packets(indexes)
        |> Stream.filter(fn packet -> packet.ref != nil end)
        |> Stream.map(&Packet.unpack(&1))
        |> Stream.zip(expected_packets)
        |> Enum.reduce(0, fn {have, want}, count ->
          assert have.pts == Map.fetch!(want, "pts")
          assert have.dts == Map.fetch!(want, "dts")
          assert byte_size(have.data) == Map.fetch!(want, "size") |> String.to_integer()
          count + 1
        end)

      assert count == length(expected_packets)
    end
  end
end
