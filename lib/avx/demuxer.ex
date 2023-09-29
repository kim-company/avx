defmodule AVx.Demuxer do
  alias AVx.{NIF, Packet}

  @schema [
    probe_size: [
      type: :non_neg_integer,
      default: 2048,
      doc:
        "Initial probe size. Will be increased if the header is not found within `probe_size` bytes iteratively, until the header is parserd"
    ],
    input: [
      type: :any,
      required: true,
      doc: "Opaque input used as parameter to read operations"
    ]
  ]

  @type input :: any()
  @type read :: (input(), size :: pos_integer -> {any(), :eof | iodata()})
  @type close :: (input() -> :ok)

  @type codec_type :: :audio | :video

  @type stream :: %{
          codec_type: codec_type(),
          codec_id: integer(),
          codec_name: binary(),
          stream_index: pos_integer(),
          codec_params: reference()
        }

  @type t :: %__MODULE__{demuxer: reference(), input: input(), eof: map()}
  defstruct [:demuxer, :input, eof: %{input: false, demuxer: false}]

  @spec new!(Keyword.t()) :: t()
  def new!(opts) do
    opts = NimbleOptions.validate!(opts, @schema)
    %__MODULE__{demuxer: NIF.demuxer_alloc_context(opts[:probe_size]), input: opts[:input]}
  end

  @doc """
  Consumes the input until the header is parsed. Then, the available streams are returned.
  """
  @spec streams(t(), read()) :: {[stream()], t()}
  def streams(state, read) do
    if NIF.demuxer_is_ready(state.demuxer) or state.eof.input do
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
    else
      state
      |> read(read)
      |> streams(read)
    end
  end

  @doc """
  Returns the packets of the selected `stream_indexes`. After using this function,
  the demuxer is left in a unusable state and hence must be discarded.

  Returns an enumerable of AVx.Packet.
  """
  @spec consume_packets(t(), [pos_integer()], read(), close()) :: Enumerable.t()
  def consume_packets(state, stream_indexes, read, close) do
    # -1 is used as an indicator for the last packet, which must
    # be delivered to the demuxer to put it into drain mode.
    accepted_streams = stream_indexes ++ [-1]

    # TODO error handling?
    Stream.resource(
      fn -> state end,
      fn
        state = %__MODULE__{eof: %{demuxer: true}} ->
          {:halt, state}

        state ->
          case NIF.demuxer_read_packet(state.demuxer) do
            :eof ->
              {[nil], %{state | eof: %{state.eof | demuxer: true}}}

            {:demand, demand} ->
              {[], read(state, read, demand)}

            {:ok, ref} ->
              {[AVx.Packet.new(ref)], state}
          end
      end,
      fn state -> close.(state.input) end
    )
    |> Stream.filter(fn packet -> Packet.stream_index(packet) in accepted_streams end)
    |> Stream.map(fn packet -> {Packet.stream_index(packet), packet} end)
  end

  defp read(state, read, demand \\ nil) do
    demand = if demand != nil, do: demand, else: NIF.demuxer_demand(state.demuxer)

    case read.(state.input, demand) do
      {:eof, input} ->
        NIF.demuxer_add_data(state.demuxer, nil)
        %{state | input: input, eof: %{state.eof | input: true}}

      {data, input} ->
        NIF.demuxer_add_data(state.demuxer, data)
        %{state | input: input}
    end
  end
end
