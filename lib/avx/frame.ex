defmodule AVx.Frame do
  @type unpacked :: %{data: binary(), pts: pos_integer()}
  @type t :: %__MODULE__{}
  defstruct [:ref]

  @spec new(reference()) :: t()
  def new(ref) do
    %__MODULE__{ref: ref}
  end

  @spec unpack_audio(t()) :: [unpacked()]
  def unpack_audio(frame) do
    {:ok, buffers} = AVx.NIF.audio_frame_unpack(frame.ref)
    buffers
  end
end
