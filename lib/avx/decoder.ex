defmodule AVx.Decoder do
  alias AVx.NIF

  defstruct [:decoder, :stream]

  def new!(stream) do
    %__MODULE__{
      decoder: NIF.decoder_alloc_context(stream.codec_id, stream.codec_params),
      stream: stream
    }
  end

  def stream_format(state) do
    state.decoder
    |> NIF.decoder_stream_format()
    |> Enum.map(fn {key, value} ->
      if is_list(value) do
        {key, to_string(value)}
      else
        {key, value}
      end
    end)
    |> Map.new()
  end

  def decode_frames(state, packets) do
    packets
    |> Stream.flat_map(fn packet ->
      key = if packet == nil, do: :eof, else: :ok
      {^key, frames} = NIF.decoder_add_data(state.decoder, packet)
      frames
    end)
  end

  def unpack_frame(frame) do
    {:ok, buffers} = NIF.unpack_frame(frame)
    Enum.map(buffers, fn x -> x.data end)
  end

  def decode_frames_unpack(state, packets) do
    state
    |> decode_frames(packets)
    |> Stream.flat_map(&unpack_frame/1)
  end
end
