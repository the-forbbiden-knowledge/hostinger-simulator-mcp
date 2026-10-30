defmodule HostingerSimulator.NativeBridge do
  @moduledoc """
  Runs the local C++ simulator as a separately isolated executable via an Erlang Port.
  This bootstrap adapter has no Phoenix or MCP networking dependencies.
  """

  @spec call(Path.t(), binary(), binary()) :: {:ok, binary()} | {:error, term()}
  def call(executable, tool, resource)
      when is_binary(executable) and is_binary(tool) and is_binary(resource) do
    cond do
      byte_size(tool) > 128 or byte_size(resource) > 256 ->
        {:error, :invalid_arguments}

      not File.regular?(executable) ->
        {:error, :native_executable_missing}

      true ->
        port =
          Port.open({:spawn_executable, String.to_charlist(Path.expand(executable))}, [
            :binary,
            :exit_status,
            :stderr_to_stdout,
            args: ["call", tool, resource]
          ])

        collect(port, [], 5_000)
    end
  end

  defp collect(port, acc, timeout) do
    receive do
      {^port, {:data, bytes}} -> collect(port, [bytes | acc], timeout)
      {^port, {:exit_status, 0}} -> {:ok, IO.iodata_to_binary(Enum.reverse(acc))}
      {^port, {:exit_status, code}} -> {:error, {:native_exit, code, IO.iodata_to_binary(Enum.reverse(acc))}}
    after
      timeout ->
        Port.close(port)
        {:error, :timeout}
    end
  end
end
