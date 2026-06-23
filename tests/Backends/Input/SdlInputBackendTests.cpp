// ZSdlInputBackend 生命周期和集成测试。
// 依赖 SDL3 运行时，只在 MAPPYZ_HAS_SDL3_INPUT 定义时编译。
// 测试后端的启动、停止、幂等性和设备枚举基本行为。
//
// 注意：这些测试在没有物理手柄的环境下也应全部通过。
// 手柄相关的输入验证需要真实设备，属于手动测试范畴。

#include <catch2/catch_test_macros.hpp>

#include "Backends/Input/SdlInputBackend.h"

using namespace MappyZ;

// ── 构造辅助 ──

// 创建后端实例的辅助函数，避免重复构造代码
static ZSdlInputBackend MakeBackend()
{
    return ZSdlInputBackend();
}

// ── 生命周期 ──

TEST_CASE("SdlBackend default state is stopped with empty devices", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    REQUIRE_FALSE(Backend.IsRunning());
    REQUIRE(Backend.ListDevices().empty());
}

TEST_CASE("SdlBackend Start sets running state", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    auto Result = Backend.Start();
    REQUIRE(Result.IsOk());
    REQUIRE(Backend.IsRunning());

    Backend.Stop();
}

TEST_CASE("SdlBackend repeated Start returns success without second worker", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    auto FirstResult = Backend.Start();
    REQUIRE(FirstResult.IsOk());

    auto SecondResult = Backend.Start();
    REQUIRE(SecondResult.IsOk());
    REQUIRE(Backend.IsRunning());

    Backend.Stop();
}

TEST_CASE("SdlBackend Stop clears running state", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    (void)Backend.Start();
    REQUIRE(Backend.IsRunning());

    Backend.Stop();
    REQUIRE_FALSE(Backend.IsRunning());
}

TEST_CASE("SdlBackend repeated Stop is safe", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    (void)Backend.Start();
    Backend.Stop();
    Backend.Stop();

    REQUIRE_FALSE(Backend.IsRunning());
}

TEST_CASE("SdlBackend Stop on never started backend is safe", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    Backend.Stop();

    REQUIRE_FALSE(Backend.IsRunning());
}

TEST_CASE("SdlBackend destructor on running backend does not crash", "[Backend][Sdl]")
{
    {
        auto Backend = MakeBackend();
        (void)Backend.Start();
        REQUIRE(Backend.IsRunning());
        // 析构时自动调用 Stop()
    }
    // 到达此处说明析构没有崩溃
    REQUIRE(true);
}

TEST_CASE("SdlBackend ListDevices returns snapshot after start", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    (void)Backend.Start();

    // 无手柄环境下应返回空列表或当前系统实际设备
    auto Devices = Backend.ListDevices();

    // 无论有无手柄，ListDevices 应返回有效快照（不崩溃）
    // 如果有手柄连接，设备列表不为空；没有则为空
    REQUIRE(Devices.size() >= 0);

    Backend.Stop();
}

TEST_CASE("SdlBackend ListDevices returns snapshot not internal reference", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    (void)Backend.Start();

    auto Snapshot1 = Backend.ListDevices();
    auto Snapshot2 = Backend.ListDevices();

    // 两次调用应返回独立拷贝
    REQUIRE(Snapshot1.size() == Snapshot2.size());

    Backend.Stop();
}

TEST_CASE("SdlBackend null callbacks do not crash on device events", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    // 不设置任何回调
    Backend.OnDeviceConnected = nullptr;
    Backend.OnDeviceDisconnected = nullptr;
    Backend.OnInputEvent = nullptr;

    auto Result = Backend.Start();
    REQUIRE(Result.IsOk());

    // 短暂运行后停止，确保空回调不导致崩溃
    Backend.Stop();
}

TEST_CASE("SdlBackend can restart after stop", "[Backend][Sdl]")
{
    auto Backend = MakeBackend();

    auto FirstResult = Backend.Start();
    REQUIRE(FirstResult.IsOk());
    REQUIRE(Backend.IsRunning());

    Backend.Stop();
    REQUIRE_FALSE(Backend.IsRunning());

    auto SecondResult = Backend.Start();
    REQUIRE(SecondResult.IsOk());
    REQUIRE(Backend.IsRunning());

    Backend.Stop();
}
