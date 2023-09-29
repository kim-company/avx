defmodule AVx.Frame do
  @type unpacked :: %{data: binary(), pts: pos_integer()}
  @type t :: %__MODULE__{}
  defstruct [:ref]

  @spec new(reference()) :: t()
  def new(ref) do
    %__MODULE__{ref: ref}
  end

  @spec unpack(t()) :: unpacked()
  def unpack(frame) do
    {:ok, buffers} = AVx.NIF.unpack_frame(frame.ref)
    buffers
  end
end
