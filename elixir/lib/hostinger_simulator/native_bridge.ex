defmodule HostingerSimulator.NativeBridge do
  @moduledoc """
  Starts the native simulator through an Erlang Port with bounded output and
  a monotonic wall-clock deadline. No network adapter or privileged operation.
  """
  @timeout_ms 5_000
  @max_output_bytes 65_536

  @spec call(Path.t(), binary(), binary()) :: {:ok, binary()} | {:error, term()}
  def call(executable, tool, resource)
      when is_binary(executable) and is_binary(tool) and is_binary(resource) do
    cond do
      not valid_tool?(tool) or not valid_resource?(resource) ->
        {:error, :invalid_arguments}

      not File.regular?(executable) ->
        {:error, :native_executable_missing}

      true ->
        try do
          port =
            Port.open({:spawn_executable, String.to_charlist(Path.expand(executable))}, [
              :binary,
              :exit_status,
              :stderr_to_stdout,
              args: ["call", tool, resource]
            ])

          deadline_ms = System.monotonic_time(:millisecond) + @timeout_ms
          collect(port, [], 0, deadline_ms)
        rescue
          ArgumentError -> {:error, :native_start_failed}
        end
    end
  end

  # Only native fixture names, never paths, URIs or arbitrary Unicode. The
  # C++ implementation remains the final enforcement point for this contract.
  defp valid_tool?(tool) do
    size = byte_size(tool)
    size > 0 and size <= 128 and String.valid?(tool) and
      Regex.match?(~r/\A[a-z][a-z0-9_.-]*\z/, tool)
  end

  defp valid_resource?(resource) do
    size = byte_size(resource)
    size > 0 and size <= 256 and String.valid?(resource) and
      Regex.match?(~r/\A[A-Za-z0-9_.:@-]+\z/, resource) and
      not String.starts_with?(resource, ".") and
      not String.ends_with?(resource, ".") and
      not String.contains?(resource, "..")
  end

  defp collect(port, chunks, size, deadline_ms) do
    remaining_ms = max(deadline_ms - System.monotonic_time(:millisecond), 0)

    if remaining_ms == 0 do
      close_port(port)
      {:error, :timeout}
    else
      receive do
        {^port, {:data, bytes}} ->
          next_size = size + byte_size(bytes)

          if next_size > @max_output_bytes do
            close_port(port)
            {:error, :response_too_large}
          else
            collect(port, [bytes | chunks], next_size, deadline_ms)
          end

        {^port, {:exit_status, 0}} ->
          {:ok, IO.iodata_to_binary(Enum.reverse(chunks))}

        {^port, {:exit_status, code}} ->
          {:error, {:native_exit, code, IO.iodata_to_binary(Enum.reverse(chunks))}}
      after
        remaining_ms ->
          close_port(port)
          {:error, :timeout}
      end
    end
  end

  defp close_port(port) do
    if Port.info(port) != nil do
      try do
        Port.close(port)
      rescue
        ArgumentError -> :ok
      end
    end

    :ok
  end
end
