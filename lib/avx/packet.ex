defmodule AVx.Packet do
  @type t :: %__MODULE__{ref: reference(), stream_index: pos_integer()}
  @type unpacked :: %{dts: integer(), pts: integer(), data: binary()}

  defstruct [:ref, :stream_index]

  @spec new(reference()) :: t()
  def new(ref) do
    %__MODULE__{ref: ref, stream_index: stream_index(ref)}
  end

  defp stream_index(nil), do: -1

  defp stream_index(ref) when is_reference(ref) do
    AVx.NIF.packet_stream_index(ref)
  end

  @spec unpack(t()) :: Packet.unpacked()
  def unpack(packet) do
    AVx.NIF.packet_unpack(packet.ref)
  end
end
