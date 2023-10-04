defmodule AVx.Decoder do
  alias AVx.{NIF, Frame}

  # NOTE
  # Right now only audio formats are supported.
  @type stream_format :: %{
          channels: pos_integer(),
          sample_rate: pos_integer(),
          sample_format: binary()
        }

  @type t :: %__MODULE__{decoder: reference(), stream: AVx.Demuxer.stream()}
  defstruct [:decoder, :stream]

  @spec new!(AVx.Demuxer.stream()) :: t()
  def new!(stream) do
    %__MODULE__{
      decoder: NIF.decoder_alloc_context(stream)
    }
  end

  @spec stream_format(t()) :: stream_format()
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

  @spec decode_frames(t(), Enumerable.t()) :: Enumerable.t()
  def decode_frames(state, packets) do
    packets
    |> Stream.flat_map(fn packet ->
      refs =
        case packet do
          nil ->
            {:eof, refs} = NIF.decoder_add_data(state.decoder, nil)
            refs

          packet ->
            {:ok, refs} = NIF.decoder_add_data(state.decoder, packet.ref)
            refs
        end

      Enum.map(refs, &Frame.new/1)
    end)
  end

  def decode_raw(state, packets) do
    state
    |> decode_frames(packets)
    |> Stream.flat_map(&Frame.unpack(&1))
    |> Stream.map(fn x -> x.data end)
  end
end
