defmodule HostingerSimulator.NativeBridgeTest do
  use ExUnit.Case

  test "missing native executable fails closed" do
    assert {:error, :native_executable_missing} =
             HostingerSimulator.NativeBridge.call("./definitely_missing.exe", "vps.list", "demo")
  end

  test "oversized arguments fail before executing a process" do
    assert {:error, :invalid_arguments} =
             HostingerSimulator.NativeBridge.call("missing.exe", "vps.list", String.duplicate("x", 257))
  end
end
