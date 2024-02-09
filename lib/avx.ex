defmodule AVx do
  @spec pts_to_secs(pos_integer(), AVx.Demuxer.stream()) :: float()
  def pts_to_secs(pts, stream) do
    pts * stream.timebase_num / stream.timebase_den
  end
end
