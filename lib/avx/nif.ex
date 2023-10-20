defmodule AVx.NIF do
  @on_load :load_nifs

  def load_nifs do
    path = :filename.join(:code.priv_dir(:avx), ~c"libav")
    :erlang.load_nif(path, 0)
  end

  def demuxer_alloc(_probe_size) do
    raise "NIF demuxer_alloc/1 not implemented"
  end

  def demuxer_alloc_from_file(_path) do
    raise "NIF demuxer_alloc_from_file/1 not implemented"
  end

  def demuxer_add_data(_ctx, _data) do
    raise "NIF demuxer_add_data/2 not implemented"
  end

  def demuxer_streams(_ctx) do
    raise "NIF demuxer_streams/1 not implemented"
  end

  def demuxer_is_ready(_ctx) do
    raise "NIF demuxer_is_ready/1 not implemented"
  end

  def demuxer_demand(_ctx) do
    raise "NIF demuxer_demand/1 not implemented"
  end

  def demuxer_read_packet(_ctx) do
    raise "NIF demuxer_read_packet/1 not implemented"
  end

  def decoder_alloc(_stream) do
    raise "NIF decoder_alloc/1 not implemented"
  end

  def decoder_add_data(_ctx, _packet) do
    raise "NIF decoder_add_data/2 not implemented"
  end

  def decoder_stream_format(_ctx) do
    raise "NIF decoder_stream_format/1 not implemented"
  end

  def packet_stream_index(_packet) do
    raise "NIF packet_stream_index/1 not implemented"
  end

  def packet_unpack(_packet) do
    raise "NIF packet_unpack/1 not implemented"
  end

  def frame_unpack(_frame) do
    raise "NIF frame_unpack/1 not implemented"
  end
end
