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

  @spec new(AVx.Demuxer.stream(), stream_format()) :: {:ok, t()} | {:error, binary()}
  def new(stream, output_format) do
    output_format = Map.update!(output_format, :sample_format, &to_charlist/1)

    case NIF.decoder_alloc(stream, output_format) do
      {:ok, decoder} ->
        decoder = %__MODULE__{
          decoder: decoder,
          stream: stream
        }

        {:ok, decoder}

      {:error, error} ->
        {:error, to_string(error)}
    end
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

  @spec decode_frames(Enumerable.t(), t()) :: Enumerable.t()
  def decode_frames(packets, state) do
    packets
    |> Stream.flat_map(fn packet ->
      refs =
        case packet.ref do
          nil ->
            {:eof, refs} = NIF.decoder_add_data(state.decoder, nil)
            refs

          ref ->
            {:ok, refs} = NIF.decoder_add_data(state.decoder, ref)
            refs
        end

      Enum.map(refs, &Frame.new/1)
    end)
  end

  def decode_raw(packets, state) do
    packets
    |> decode_frames(state)
    |> Stream.map(&Frame.unpack_audio(&1))
    |> Stream.map(fn x -> x.data end)
  end
end
