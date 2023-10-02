defmodule AVx.DemuxerTest do
  use ExUnit.Case

  alias AVx.{Demuxer, Packet}
  alias AVx.Demuxer.MailboxReader

  @input "test/data/mic.mp4"

  @tag :tmp_dir
  test "demuxes aac track", %{tmp_dir: tmp_dir} do
    input = File.open!(@input, [:raw, :read])
    output_path = Path.join([tmp_dir, "output.aac"])
    output = File.stream!(output_path, [:raw, :write])

    demuxer = Demuxer.new!(probe_size: 2048, input: input)

    read = fn input, size ->
      resp = IO.binread(input, size)
      {resp, input}
    end

    close = fn input -> File.close(input) end

    {streams, demuxer} = Demuxer.streams(demuxer, read)
    stream = Enum.find(streams, fn stream -> stream.codec_type == :audio end)

    # TODO
    # Why is this file not playable by ffplay out of the box? Did
    # the demuxer strip away headers or other relevant info?

    demuxer
    |> Demuxer.consume_packets([stream.stream_index], read, close)
    |> collect_data(output)

    assert File.stat!(output_path).size > 0
  end

  @tag :tmp_dir
  test "with MailboxReader", %{tmp_dir: tmp_dir} do
    pid = start_link_supervised!(MailboxReader)
    input = File.stream!(@input, [:raw, :read])

    spawn_link(fn ->
      input
      |> Enum.map(fn data ->
        send(pid, {:data, data})
      end)

      send(pid, {:data, nil})
    end)

    output_path = Path.join([tmp_dir, "output.aac"])
    output = File.stream!(output_path, [:raw, :write])

    demuxer = Demuxer.new!(probe_size: 2048, input: pid)
    {streams, demuxer} = Demuxer.streams(demuxer, &MailboxReader.read/2)
    stream = Enum.find(streams, fn stream -> stream.codec_type == :audio end)

    demuxer
    |> Demuxer.consume_packets(
      [stream.stream_index],
      &MailboxReader.read/2,
      &MailboxReader.close/1
    )
    |> collect_data(output)

    assert File.stat!(output_path).size > 0
  end

  defp collect_data(packets, output) do
    packets
    |> Stream.map(fn {_, packet} -> packet end)
    |> Stream.filter(fn packet -> packet != nil end)
    |> Stream.map(&Packet.unpack/1)
    |> Stream.map(fn unpacked -> unpacked.data end)
    |> Enum.into(output)
  end
end
