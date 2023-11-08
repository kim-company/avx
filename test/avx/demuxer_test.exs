defmodule AVx.DemuxerTest do
  use ExUnit.Case

  alias AVx.{Demuxer, Packet}

  @input "test/data/mic.mp4"

  describe "demuxer" do
    test "from file" do
      assert_demuxer(@input)
    end

    test "from tcp socket" do
      pid =
        start_link_supervised!(
          {ThousandIsland,
           [port: 0, handler_module: Support.Handler, handler_options: %{path: @input}]}
        )

      {:ok, {_, port}} = ThousandIsland.listener_info(pid)
      addr = "tcp://127.0.0.1:#{port}"

      assert_demuxer(addr)
      ThousandIsland.stop(pid)
    end

    defp assert_demuxer(input_path) do
      {:ok, demuxer} = Demuxer.new_from_file(input_path)
      streams = Demuxer.read_streams(demuxer)

      stream = Enum.find(streams, fn stream -> Demuxer.stream_type(stream) == :audio end)

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
          assert byte_size(have.payload) == Map.fetch!(want, "size") |> String.to_integer()
          count + 1
        end)

      assert count == length(expected_packets)
    end
  end
end
