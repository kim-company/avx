defmodule AVx.Demuxer.MailboxReader do
  use GenServer

  def start_link(opts \\ []) do
    GenServer.start_link(__MODULE__, [], opts)
  end

  def read(server, size) do
    # Blocks util a response is received.
    case GenServer.call(server, {:read, size}, :infinity) do
      {:ok, data} ->
        {data, server}

      {:eof} ->
        {:eof, server}
    end
  end

  def close(pid) do
    Process.exit(pid, :normal)
  end

  @impl GenServer
  def init(_opts) do
    {:ok, %{pending: nil, buffer: <<>>, eof: false}}
  end

  @impl true
  def handle_call({:read, size}, from, state) do
    cond do
      state.eof and byte_size(state.buffer) == 0 ->
        {:reply, :eof, state}

      byte_size(state.buffer) == 0 ->
        {:noreply, %{state | pending: {from, size}}}

      true ->
        {buffer, state} = read_buffer(state, size)
        {:reply, {:ok, buffer}, state}
    end
  end

  @impl GenServer
  def handle_info({:data, nil}, state) do
    {:noreply, %{state | eof: true}}
  end

  def handle_info({:data, data}, state = %{pending: nil}) do
    {:noreply, %{state | buffer: state.buffer <> data}}
  end

  def handle_info({:data, data}, state = %{pending: {from, size}}) do
    state = %{state | buffer: state.buffer <> data}
    {buffer, state} = read_buffer(state, size)
    GenServer.reply(from, {:ok, buffer})

    {:noreply, state}
  end

  defp read_buffer(state, size) do
    if byte_size(state.buffer) >= size do
      <<buf::binary-size(size), rest::binary>> = state.buffer
      {buf, %{state | pending: nil, buffer: rest}}
    else
      {state.buffer, %{state | pending: nil, buffer: <<>>}}
    end
  end
end
