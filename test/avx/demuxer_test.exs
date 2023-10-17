defmodule AVx.DemuxerTest do
  use ExUnit.Case

  alias AVx.{Demuxer, Packet}
  alias AVx.Demuxer.MailboxReader

  @input "test/data/mic.mp4"

  describe "demuxer" do
    test "from file" do
      demuxer = Demuxer.new_from_file(@input)

      {streams, demuxer} = Demuxer.streams(demuxer)

      stream = Enum.find(streams, fn stream -> stream.codec_type == :audio end)

      info = Support.show_packets(@input)
      assert_packets(demuxer, [stream.stream_index], info)
    end

    test "with bin read from memory" do
      demuxer =
        Demuxer.new_in_memory(%{
          opaque: File.open!(@input, [:raw, :read]),
          read: fn input, size ->
            resp = IO.binread(input, size)
            {resp, input}
          end,
          close: fn input -> File.close(input) end
        })

      {streams, demuxer} = Demuxer.streams(demuxer)

      stream = Enum.find(streams, fn stream -> stream.codec_type == :audio end)

      info = Support.show_packets(@input)
      assert_packets(demuxer, [stream.stream_index], info)
    end

    test "with MailboxReader" do
      pid = start_link_supervised!(MailboxReader)

      spawn_link(fn ->
        @input
        |> File.stream!([:raw, :read], 2048)
        |> Enum.map(fn data ->
          send(pid, {:data, data})
        end)

        send(pid, {:data, nil})
      end)

      demuxer =
        Demuxer.new_in_memory(%{
          opaque: pid,
          read: &MailboxReader.read/2,
          close: &MailboxReader.close/1
        })

      {streams, demuxer} = Demuxer.streams(demuxer)

      stream = Enum.find(streams, fn stream -> stream.codec_type == :audio end)

      info = Support.show_packets(@input)
      assert_packets(demuxer, [stream.stream_index], info)
    end

    defp assert_packets(demuxer, indexes, expected_packets) do
      count =
        demuxer
        |> Demuxer.consume_packets(indexes)
        |> Stream.map(fn {_, packet} -> packet end)
        |> Stream.filter(fn packet -> packet != nil end)
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
