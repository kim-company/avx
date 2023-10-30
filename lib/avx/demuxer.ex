defmodule AVx.Demuxer do
  alias AVx.{NIF, Packet}
  require Logger

  @type codec_type :: :audio | :video

  @type stream :: %{
          codec_type: codec_type(),
          codec_id: integer(),
          codec_name: binary(),
          stream_index: pos_integer(),
          codec_params: reference(),
          timebase_num: pos_integer(),
          timebase_den: pos_integer()
        }

  @type t :: %__MODULE__{
          demuxer: reference(),
          uri: URI.t(),
          eof: map()
        }
  defstruct [:demuxer, :uri, eof: %{input: false, demuxer: false}]

  @spec new_from_file(String.t()) :: {:ok, t()} | {:error, any()}
  def new_from_file(uri) do
    case NIF.demuxer_alloc_from_file(uri) do
      {:ok, demuxer} -> {:ok, %__MODULE__{demuxer: demuxer, uri: uri}}
      {:error, reason} -> {:error, to_string(reason)}
    end
  end

  @doc """
  Opens and starts reading the input file finding the streams
  contained in it.
  """
  @spec read_streams(t()) :: [stream()]
  def read_streams(state) do
    state.demuxer
    |> NIF.demuxer_streams()
    |> Enum.map(fn stream ->
      # Coming from erlang, strings are charlists.
      stream
      |> Enum.map(fn {key, value} ->
        if is_list(value) do
          {key, to_string(value)}
        else
          {key, value}
        end
      end)
      |> Map.new()
    end)
  end

  @doc """
  Returns the packets of the selected `stream_indexes`. After using this function,
  the demuxer is left in a unusable state and hence must be discarded.

  Returns an enumerable of AVx.Packet.
  """
  @spec consume_packets(t(), [pos_integer()]) :: Stream.t()
  def consume_packets(state, stream_indexes) when is_list(stream_indexes) do
    # -1 is used as an indicator for the last packet, which must
    # be delivered to the decoder to put it into drain mode.
    accepted_streams = stream_indexes ++ [-1]

    do_consume_packets(state, accepted_streams)
  end

  defp do_consume_packets(state, stream_indexes) do
    Stream.resource(
      fn -> state end,
      fn
        state = %__MODULE__{eof: %{demuxer: true}} ->
          {:halt, state}

        state ->
          case NIF.demuxer_read_packet(state.demuxer) do
            {:error, reason} ->
              Logger.error("Demuxer read packet failed: #{inspect(to_string(reason))}")
              {[AVx.Packet.new(nil)], %{state | eof: %{state.eof | demuxer: true}}}

            :eof ->
              {[AVx.Packet.new(nil)], %{state | eof: %{state.eof | demuxer: true}}}

            {:ok, ref} ->
              {[AVx.Packet.new(ref)], state}
          end
      end,
      fn _state -> :ok end
    )
    |> filter_packets(stream_indexes)
  end

  defp filter_packets(packets, accepted_streams) do
    packets
    |> Stream.filter(fn packet -> packet.stream_index in accepted_streams end)
  end

  def consume_raw(state, stream_indexes) do
    state
    |> consume_packets(stream_indexes)
    |> Stream.filter(fn packet -> packet.ref != nil end)
    |> Stream.map(&Packet.unpack/1)
    |> Stream.map(fn packet -> packet.data end)
  end
end
