defmodule Support do
  def show_packets_and_frames(path) do
    (path <> ".json")
    |> File.read!()
    |> Jason.decode!()
    |> Map.fetch!("packets_and_frames")
  end

  def show_packets(path) do
    path
    |> show_packets_and_frames()
    |> Enum.filter(fn %{"type" => type} -> type == "packet" end)
  end

  def show_frames(path) do
    path
    |> show_packets_and_frames()
    |> Enum.filter(fn %{"type" => type} -> type == "frame" end)
  end
end

defmodule Support.Handler do
  use ThousandIsland.Handler

  @impl ThousandIsland.Handler
  def handle_connection(socket, state) do
    data = File.read!(state.path)
    ThousandIsland.Socket.send(socket, data)
    {:close, state}
  end
end
