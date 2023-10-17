defmodule AVx.Demuxer do
  alias AVx.{NIF, Packet}
  @default_probe_size 2048

  @type read :: (any(), size :: pos_integer -> {:eof | iodata(), any()})
  @type close :: (any() -> :ok)
  @type input :: any()

  @type reader :: %{
          read: read(),
          close: close(),
          opaque: any(),
          probe_size: non_neg_integer()
        }

  @type codec_type :: :audio | :video

  @type stream :: %{
          codec_type: codec_type(),
          codec_id: integer(),
          codec_name: binary(),
          stream_index: pos_integer(),
          codec_params: reference()
        }

  @type t :: %__MODULE__{
          demuxer: reference(),
          reader: reader() | nil,
          file_path: Path.t() | nil,
          eof: map()
        }
  defstruct [:demuxer, :reader, :file_path, eof: %{input: false, demuxer: false}]

  @spec new_in_memory(reader()) :: t()
  def new_in_memory(reader) do
    probe_size = Map.get(reader, :probe_size, @default_probe_size)
    %__MODULE__{demuxer: NIF.demuxer_alloc(probe_size), reader: reader}
  end

  @spec new_from_file(Path.t()) :: t()
  def new_from_file(path) do
    %__MODULE__{demuxer: NIF.demuxer_alloc_from_file(path), file_path: path}
  end

  @doc """
  Consumes the input until the header is parsed. Then, the available streams are returned.
  """
  @spec streams(t()) :: {[stream()], t()}
  def streams(state = %{reader: reader}) when reader != nil do
    if is_ready(state) or state.eof.input do
      read_streams(state)
    else
      state
      |> read_input()
      |> streams()
    end
  end

  def streams(state) do
    read_streams(state)
  end

  defp is_ready(state) do
    NIF.demuxer_is_ready(state.demuxer) != 0
  end

  @doc """
  Returns the packets of the selected `stream_indexes`. After using this function,
  the demuxer is left in a unusable state and hence must be discarded.

  Returns an enumerable of AVx.Packet.
  """
  @spec consume_packets(t(), [pos_integer()]) :: Enumerable.t()
  def consume_packets(state = %{reader: reader}, stream_indexes) when reader != nil do
    # -1 is used as an indicator for the last packet, which must
    # be delivered to the decoder to put it into drain mode.
    accepted_streams = stream_indexes ++ [-1]

    # TODO error handling?
    Stream.resource(
      fn -> state end,
      fn
        state = %__MODULE__{eof: %{demuxer: true}} ->
          {:halt, state}

        state ->
          case NIF.demuxer_read_packet(state.demuxer) do
            {:error, _reason} ->
              {[nil], %{state | eof: %{state.eof | demuxer: true}}}

            :eof ->
              {[nil], %{state | eof: %{state.eof | demuxer: true}}}

            {:demand, demand} ->
              {[], read_input(state, demand)}

            {:ok, ref} ->
              {[AVx.Packet.new(ref)], state}
          end
      end,
      fn
        {:error, reason} -> raise reason
        state -> state.reader.close.(state.reader.opaque)
      end
    )
    |> filter_packets(accepted_streams)
  end

  def consume_packets(state, stream_indexes) do
    # -1 is used as an indicator for the last packet, which must
    # be delivered to the decoder to put it into drain mode.
    accepted_streams = stream_indexes ++ [-1]

    Stream.resource(
      fn -> state end,
      fn
        state = %__MODULE__{eof: %{demuxer: true}} ->
          {:halt, state}

        state ->
          # There is no demand when the demuxer is taking care of
          # the input internally.
          case NIF.demuxer_read_packet(state.demuxer) do
            {:error, _reason} ->
              {[nil], %{state | eof: %{state.eof | demuxer: true}}}

            :eof ->
              {[nil], %{state | eof: %{state.eof | demuxer: true}}}

            {:ok, ref} ->
              {[AVx.Packet.new(ref)], state}
          end
      end,
      fn _state -> :ok end
    )
    |> filter_packets(accepted_streams)
  end

  defp filter_packets(packets, accepted_streams) do
    packets
    |> Stream.map(fn packet -> {Packet.stream_index(packet), packet} end)
    |> Stream.filter(fn {stream_index, _packet} -> stream_index in accepted_streams end)
  end

  def consume_raw(state, stream_indexes) do
    state
    |> consume_packets(stream_indexes)
    |> Stream.map(fn {_, packet} -> packet end)
    |> Stream.filter(fn packet -> packet != nil end)
    |> Stream.map(&Packet.unpack/1)
    |> Stream.map(fn packet -> packet.data end)
  end

  defp read_input(state, demand \\ nil) do
    demand = if demand != nil, do: demand, else: NIF.demuxer_demand(state.demuxer)

    case state.reader.read.(state.reader.opaque, demand) do
      {:eof, opaque} ->
        NIF.demuxer_add_data(state.demuxer, nil)
        %{state | reader: %{state.reader | opaque: opaque}, eof: %{state.eof | input: true}}

      {data, opaque} ->
        NIF.demuxer_add_data(state.demuxer, data)
        %{state | reader: %{state.reader | opaque: opaque}}
    end
  end

  defp read_streams(state) do
    streams =
      state.demuxer
      |> NIF.demuxer_streams()
      # TODO error handling?
      |> elem(1)
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

    {streams, state}
  end
end
