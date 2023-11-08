defmodule AVx.Packet do
  @type t :: %__MODULE__{
          ref: reference(),
          payload: nil | binary(),
          stream_index: pos_integer(),
          pts: pos_integer(),
          dts: pos_integer()
        }

  defstruct ref: nil, payload: nil, stream_index: -1, pts: -1, dts: -1

  @spec new(reference()) :: t()
  def new(nil) do
    %__MODULE__{}
  end

  def new(ref) do
    meta = AVx.NIF.packet_metadata(ref)
    %__MODULE__{ref: ref, stream_index: meta.stream_index, pts: meta.pts, dts: meta.dts}
  end

  @spec unpack(t()) :: t()
  def unpack(packet) do
    %__MODULE__{packet | payload: AVx.NIF.packet_unpack(packet.ref)}
  end
end
