defmodule HostingerSimulator.NativeBridgeTest do
  use ExUnit.Case, async: false

  alias HostingerSimulator.NativeBridge

  test "missing native executable fails closed" do
    assert {:error, :native_executable_missing} =
             NativeBridge.call("./definitely_missing.exe", "vps.list", "demo")
  end

  test "oversized arguments fail before executing a process" do
    assert {:error, :invalid_arguments} =
             NativeBridge.call("missing.exe", "vps.list", String.duplicate("x", 257))
  end

  test "real native binary returns bounded offline preview when available" do
    case System.get_env("SIMULATOR_NATIVE_EXE") do
      nil ->
        assert true

      executable ->
        assert {:ok, response} = NativeBridge.call(executable, "monitoring.health", "safe_fixture")
        assert response =~ ~s("ok":true)
        assert response =~ ~s("mode":"simulation_only")
        assert response =~ ~s("side_effects":false)
      end
  end

  if match?({:unix, _}, :os.type()) do
    setup do
      filename =
        Path.join(
          System.tmp_dir!(),
          "native_bridge_test_#{System.unique_integer([:positive])}.sh"
        )

      File.write!(
        filename,
        """
        #!/bin/sh
        case "$3" in
          success) printf 'safe-output' ;;
          failure) printf 'fixture-failed'; exit 7 ;;
          oversized) dd if=/dev/zero bs=1024 count=80 2>/dev/null | tr '\\000' x ;;
          trickle)
            i=0
            while [ "$i" -lt 12 ]; do
              printf 'x'
              sleep 1
              i=$((i + 1))
            done
            ;;
        esac
        """
      )

      File.chmod!(filename, 0o700)
      on_exit(fn -> File.rm(filename) end)
      {:ok, fixture: filename}
    end

    test "returns successful bounded output", %{fixture: fixture} do
      assert {:ok, "safe-output"} = NativeBridge.call(fixture, "tool", "success")
    end

    test "preserves nonzero native exit without reporting success", %{fixture: fixture} do
      assert {:error, {:native_exit, 7, "fixture-failed"}} =
               NativeBridge.call(fixture, "tool", "failure")
    end

    test "rejects excessive native output", %{fixture: fixture} do
      assert {:error, :response_too_large} = NativeBridge.call(fixture, "tool", "oversized")
    end

    test "enforces absolute deadline despite repeated output", %{fixture: fixture} do
      started = System.monotonic_time(:millisecond)
      assert {:error, :timeout} = NativeBridge.call(fixture, "tool", "trickle")
      elapsed = System.monotonic_time(:millisecond) - started
      assert elapsed >= 4_000
      assert elapsed < 7_000
    end
  end
end
