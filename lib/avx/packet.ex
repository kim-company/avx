defmodule AVx.Packet do
  @type t :: %__MODULE__{}
  defstruct [:ref]

  @spec new(reference()) :: t()
  def new(ref) do
    %__MODULE__{ref: ref}
  end

  @spec stream_index(nil | t()) :: integer()
  def stream_index(nil), do: -1

  def stream_index(packet) do
    AVx.NIF.packet_stream_index(packet.ref)
  end
end
