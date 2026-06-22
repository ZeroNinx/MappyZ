// ZSdlInputBackend 实现。
// 通过 SDL3 Gamepad API 枚举手柄、监听热插拔和读取输入事件，
// 将 SDL 事件转换为项目内部 SDeviceInfo 和 SInputEvent。
//
// SDL 初始化和设备枚举在 Start() 调用线程完成，
// 事件轮询在独立 worker 线程中进行。
// 所有非预期路径都输出日志以方便定位问题。

#include "Backends/Input/SdlInputBackend.h"

#include "Backends/Input/SdlInputHelpers.h"
#include "Core/ControlId.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

// ── 枚举镜像值校验 ──
// 确保 SdlInputHelpers 中的常量与 SDL3 实际定义一致

static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::South == SDL_GAMEPAD_BUTTON_SOUTH,
    "SdlButton::South mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::East == SDL_GAMEPAD_BUTTON_EAST,
    "SdlButton::East mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::West == SDL_GAMEPAD_BUTTON_WEST,
    "SdlButton::West mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::North == SDL_GAMEPAD_BUTTON_NORTH,
    "SdlButton::North mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::Back == SDL_GAMEPAD_BUTTON_BACK,
    "SdlButton::Back mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::Guide == SDL_GAMEPAD_BUTTON_GUIDE,
    "SdlButton::Guide mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::Start == SDL_GAMEPAD_BUTTON_START,
    "SdlButton::Start mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::LeftStick == SDL_GAMEPAD_BUTTON_LEFT_STICK,
    "SdlButton::LeftStick mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::RightStick == SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    "SdlButton::RightStick mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::LeftShoulder == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    "SdlButton::LeftShoulder mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::RightShoulder == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    "SdlButton::RightShoulder mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::DpadUp == SDL_GAMEPAD_BUTTON_DPAD_UP,
    "SdlButton::DpadUp mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::DpadDown == SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    "SdlButton::DpadDown mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::DpadLeft == SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    "SdlButton::DpadLeft mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlButton::DpadRight == SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    "SdlButton::DpadRight mismatch");

static_assert(
    ZeroMapper::SdlInputHelpers::SdlAxis::LeftX == SDL_GAMEPAD_AXIS_LEFTX,
    "SdlAxis::LeftX mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlAxis::LeftY == SDL_GAMEPAD_AXIS_LEFTY,
    "SdlAxis::LeftY mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlAxis::RightX == SDL_GAMEPAD_AXIS_RIGHTX,
    "SdlAxis::RightX mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlAxis::RightY == SDL_GAMEPAD_AXIS_RIGHTY,
    "SdlAxis::RightY mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlAxis::LeftTrigger == SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    "SdlAxis::LeftTrigger mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlAxis::RightTrigger == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
    "SdlAxis::RightTrigger mismatch");

static_assert(
    ZeroMapper::SdlInputHelpers::SdlGamepadEvent::Added == SDL_EVENT_GAMEPAD_ADDED,
    "SdlGamepadEvent::Added mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlGamepadEvent::Removed == SDL_EVENT_GAMEPAD_REMOVED,
    "SdlGamepadEvent::Removed mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlGamepadEvent::Remapped == SDL_EVENT_GAMEPAD_REMAPPED,
    "SdlGamepadEvent::Remapped mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlGamepadEvent::ButtonDown == SDL_EVENT_GAMEPAD_BUTTON_DOWN,
    "SdlGamepadEvent::ButtonDown mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlGamepadEvent::ButtonUp == SDL_EVENT_GAMEPAD_BUTTON_UP,
    "SdlGamepadEvent::ButtonUp mismatch");
static_assert(
    ZeroMapper::SdlInputHelpers::SdlGamepadEvent::AxisMotion == SDL_EVENT_GAMEPAD_AXIS_MOTION,
    "SdlGamepadEvent::AxisMotion mismatch");

namespace ZeroMapper
{

// ── 内部辅助函数 ──

// 根据 SDL 设备信息构建项目内部 SDeviceInfo
static SDeviceInfo BuildDeviceInfo(SDL_JoystickID InstanceId, SDL_Gamepad* Handle)
{
    SDeviceInfo Info;

    // 设备 ID: "sdl3:<instance_id>"，会话内唯一
    Info.Id.Value = "sdl3:" + std::to_string(static_cast<uint32>(InstanceId));

    // 后端标识
    Info.Backend = "sdl3";

    // SDL 实例 ID 的字符串形式
    Info.InstanceId = std::to_string(static_cast<uint32>(InstanceId));

    // 设备名称
    const char* Name = SDL_GetGamepadName(Handle);
    Info.Name = Name ? Name : "";

    // GUID 字符串
    SDL_Joystick* Joystick = SDL_GetGamepadJoystick(Handle);
    if (Joystick)
    {
        SDL_GUID Guid = SDL_GetJoystickGUID(Joystick);
        char GuidBuffer[33] = {};
        SDL_GUIDToString(Guid, GuidBuffer, sizeof(GuidBuffer));
        Info.Guid = GuidBuffer;
    }

    // Vendor / Product ID，使用 4 位小写十六进制
    Uint16 VendorId = SDL_GetGamepadVendor(Handle);
    Uint16 ProductId = SDL_GetGamepadProduct(Handle);

    if (VendorId != 0)
    {
        char Buffer[5] = {};
        std::snprintf(Buffer, sizeof(Buffer), "%04x", static_cast<unsigned>(VendorId));
        Info.VendorId = Buffer;
    }

    if (ProductId != 0)
    {
        char Buffer[5] = {};
        std::snprintf(Buffer, sizeof(Buffer), "%04x", static_cast<unsigned>(ProductId));
        Info.ProductId = Buffer;
    }

    return Info;
}

// ── Pimpl 实现结构 ──

// 单个手柄的运行时状态
struct SGamepadState
{
    SDL_Gamepad* Handle = nullptr;
    SDeviceInfo Info;

    // 摇杆轴值缓存，用于合并 X/Y 单轴事件为完整 Axis2D 事件
    SAxis2DValue LeftStickCache;
    SAxis2DValue RightStickCache;
};

struct ZSdlInputBackend::SImpl
{
    // 指向拥有者，用于访问回调函数
    ZSdlInputBackend& Owner;

    // worker 线程，使用 jthread 支持协作式停止
    std::jthread WorkerThread;

    // 保护 Devices 列表，ListDevices() 可能从其他线程调用
    mutable std::mutex DeviceMutex;

    // 当前已连接设备信息列表，供 ListDevices() 返回快照
    TVector<SDeviceInfo> Devices;

    // 已打开手柄的运行时状态，key 为 SDL joystick instance ID。
    // 只从 worker 线程和 Start()/Stop() 中访问，不需要额外同步。
    THashMap<uint32, SGamepadState> Gamepads;

    // 运行状态标志，跨线程读写
    std::atomic<bool> bRunning{false};

    // SDL gamepad 子系统是否已初始化
    bool bSdlInitialized = false;

    explicit SImpl(ZSdlInputBackend& InOwner)
        : Owner(InOwner)
    {
    }

    // ── Worker 线程主函数 ──

    void WorkerMain(std::stop_token StopToken);

    // ── 事件处理 ──

    void HandleSdlEvent(const SDL_Event& Event);
    void HandleGamepadAdded(SDL_JoystickID InstanceId);
    void HandleGamepadRemoved(SDL_JoystickID InstanceId);
    void HandleGamepadRemapped(SDL_JoystickID InstanceId);
    void HandleGamepadButton(SDL_JoystickID InstanceId, int Button, bool bPressed);
    void HandleGamepadAxis(SDL_JoystickID InstanceId, int Axis, int16 RawValue);

    // ── 生命周期辅助 ──

    // 枚举并打开当前已连接的手柄
    void EnumerateExistingGamepads();

    // 释放所有 SDL 资源（手柄 handle、子系统）
    void CleanupSdl();

    // 向 Devices 列表中添加一个设备信息（加锁）
    void AddDeviceInfo(const SDeviceInfo& DeviceInfo);

    // 从 Devices 列表中移除指定 ID 的设备信息（加锁）
    void RemoveDeviceInfo(const SDeviceId& DeviceId);
};

// ── SImpl 方法实现 ──

void ZSdlInputBackend::SImpl::WorkerMain(std::stop_token StopToken)
{
    while (!StopToken.stop_requested())
    {
        SDL_Event Event;

        // 等待事件或超时，超时间隔 8ms 以便定期检查停止请求
        if (SDL_WaitEventTimeout(&Event, 8))
        {
            HandleSdlEvent(Event);

            // 排空当前事件队列，避免事件突发时逐个等待超时
            while (SDL_PollEvent(&Event))
            {
                if (StopToken.stop_requested())
                {
                    return;
                }
                HandleSdlEvent(Event);
            }
        }
    }
}

void ZSdlInputBackend::SImpl::HandleSdlEvent(const SDL_Event& Event)
{
    using SdlInputHelpers::ClassifyGamepadEvent;
    using SdlInputHelpers::ESdlGamepadAction;

    switch (ClassifyGamepadEvent(Event.type))
    {
    case ESdlGamepadAction::DeviceAdded:
        HandleGamepadAdded(Event.gdevice.which);
        break;
    case ESdlGamepadAction::DeviceRemoved:
        HandleGamepadRemoved(Event.gdevice.which);
        break;
    case ESdlGamepadAction::DeviceRemapped:
        HandleGamepadRemapped(Event.gdevice.which);
        break;
    case ESdlGamepadAction::ButtonInput:
        HandleGamepadButton(
            Event.gbutton.which,
            Event.gbutton.button,
            Event.type == SdlInputHelpers::SdlGamepadEvent::ButtonDown);
        break;
    case ESdlGamepadAction::AxisInput:
        HandleGamepadAxis(Event.gaxis.which, Event.gaxis.axis, Event.gaxis.value);
        break;
    case ESdlGamepadAction::Ignored:
        break;
    }
}

void ZSdlInputBackend::SImpl::HandleGamepadAdded(SDL_JoystickID InstanceId)
{
    auto Key = static_cast<uint32>(InstanceId);

    // 重复 added 事件（包括 remapped 后重复触发），不重复处理
    if (Gamepads.contains(Key))
    {
        std::fprintf(stderr,
            "[SdlInputBackend] 调试: 收到重复的 GAMEPAD_ADDED 事件 (instance_id: %u)，忽略\n",
            Key);
        return;
    }

    // 尝试打开设备
    SDL_Gamepad* Handle = SDL_OpenGamepad(InstanceId);
    if (!Handle)
    {
        std::fprintf(stderr,
            "[SdlInputBackend] 警告: 无法打开手柄 (instance_id: %u): %s\n",
            Key, SDL_GetError());
        return;
    }

    // 构建设备信息并记录到内部状态
    SDeviceInfo DeviceInfo = BuildDeviceInfo(InstanceId, Handle);

    SGamepadState State;
    State.Handle = Handle;
    State.Info = DeviceInfo;
    Gamepads[Key] = std::move(State);

    AddDeviceInfo(DeviceInfo);

    std::fprintf(stderr,
        "[SdlInputBackend] 信息: 手柄已连接 \"%s\" (id: \"%s\")\n",
        DeviceInfo.Name.c_str(), DeviceInfo.Id.Value.c_str());

    // 触发连接回调
    if (Owner.OnDeviceConnected)
    {
        Owner.OnDeviceConnected(DeviceInfo);
    }
}

void ZSdlInputBackend::SImpl::HandleGamepadRemoved(SDL_JoystickID InstanceId)
{
    auto Key = static_cast<uint32>(InstanceId);

    auto Iterator = Gamepads.find(Key);
    if (Iterator == Gamepads.end())
    {
        std::fprintf(stderr,
            "[SdlInputBackend] 警告: 收到未知设备的 GAMEPAD_REMOVED 事件 (instance_id: %u)\n",
            Key);
        return;
    }

    SDeviceId DeviceId = Iterator->second.Info.Id;
    StdString DeviceName = Iterator->second.Info.Name;

    // 关闭 SDL handle 并移除内部记录
    if (Iterator->second.Handle)
    {
        SDL_CloseGamepad(Iterator->second.Handle);
    }
    Gamepads.erase(Iterator);

    RemoveDeviceInfo(DeviceId);

    std::fprintf(stderr,
        "[SdlInputBackend] 信息: 手柄已断开 \"%s\" (id: \"%s\")\n",
        DeviceName.c_str(), DeviceId.Value.c_str());

    // 触发断开回调
    if (Owner.OnDeviceDisconnected)
    {
        Owner.OnDeviceDisconnected(DeviceId);
    }
}

void ZSdlInputBackend::SImpl::HandleGamepadRemapped(SDL_JoystickID InstanceId)
{
    auto Key = static_cast<uint32>(InstanceId);

    // 第一版只记录调试信息，不重开设备、不触发 connected 回调
    auto Iterator = Gamepads.find(Key);
    if (Iterator == Gamepads.end())
    {
        std::fprintf(stderr,
            "[SdlInputBackend] 调试: 收到未知设备的 GAMEPAD_REMAPPED 事件 (instance_id: %u)\n",
            Key);
        return;
    }

    std::fprintf(stderr,
        "[SdlInputBackend] 调试: 手柄映射已更新 \"%s\" (instance_id: %u)\n",
        Iterator->second.Info.Name.c_str(), Key);
}

void ZSdlInputBackend::SImpl::HandleGamepadButton(
    SDL_JoystickID InstanceId,
    int Button,
    bool bPressed)
{
    auto Key = static_cast<uint32>(InstanceId);

    auto GamepadIterator = Gamepads.find(Key);
    if (GamepadIterator == Gamepads.end())
    {
        std::fprintf(stderr,
            "[SdlInputBackend] 警告: 收到未知设备的按钮事件 (instance_id: %u)\n", Key);
        return;
    }

    // 映射 SDL 按钮到项目内部 ControlId
    auto MappedControlId = SdlInputHelpers::MapButtonToControlId(Button);
    if (!MappedControlId.has_value())
    {
        std::fprintf(stderr,
            "[SdlInputBackend] 调试: 不支持的 SDL 按钮类型 %d (instance_id: %u)\n",
            Button, Key);
        return;
    }

    // 构建输入事件
    SInputEvent Event;
    Event.DeviceId = GamepadIterator->second.Info.Id;
    Event.ControlId = StdString(MappedControlId.value());
    Event.ControlType = EInputControlType::Button;
    Event.EventType = bPressed ? EInputEventType::Pressed : EInputEventType::Released;
    Event.Value = bPressed ? 1.0f : 0.0f;
    Event.Timestamp = std::chrono::steady_clock::now();

    if (Owner.OnInputEvent)
    {
        Owner.OnInputEvent(Event);
    }
}

void ZSdlInputBackend::SImpl::HandleGamepadAxis(
    SDL_JoystickID InstanceId,
    int Axis,
    int16 RawValue)
{
    auto Key = static_cast<uint32>(InstanceId);

    auto GamepadIterator = Gamepads.find(Key);
    if (GamepadIterator == Gamepads.end())
    {
        std::fprintf(stderr,
            "[SdlInputBackend] 警告: 收到未知设备的轴事件 (instance_id: %u)\n", Key);
        return;
    }

    // 映射 SDL 轴到项目内部轴信息
    auto AxisMapping = SdlInputHelpers::MapAxisToMapping(Axis);
    if (!AxisMapping.has_value())
    {
        std::fprintf(stderr,
            "[SdlInputBackend] 调试: 不支持的 SDL 轴类型 %d (instance_id: %u)\n",
            Axis, Key);
        return;
    }

    const auto& Mapping = AxisMapping.value();
    auto& GamepadState = GamepadIterator->second;

    SInputEvent Event;
    Event.DeviceId = GamepadState.Info.Id;
    Event.ControlId = StdString(Mapping.ControlId);
    Event.ControlType = Mapping.ControlType;
    Event.EventType = EInputEventType::Changed;
    Event.Timestamp = std::chrono::steady_clock::now();

    if (Mapping.ControlType == EInputControlType::Trigger)
    {
        // 扳机：直接归一化输出
        Event.Value = SdlInputHelpers::NormalizeTriggerAxis(RawValue);
    }
    else if (Mapping.ControlType == EInputControlType::Axis2D)
    {
        // 摇杆：合并 X/Y 轴缓存后输出完整双轴值
        float32 NormalizedValue = SdlInputHelpers::NormalizeStickAxis(RawValue);

        // 选择对应摇杆的缓存
        SAxis2DValue& Cache = (Mapping.ControlId == ControlId::LeftStick)
            ? GamepadState.LeftStickCache
            : GamepadState.RightStickCache;

        Event.Axis2D = SdlInputHelpers::MergeAxis2DCache(Cache, Mapping.bIsXAxis, NormalizedValue);
    }

    if (Owner.OnInputEvent)
    {
        Owner.OnInputEvent(Event);
    }
}

void ZSdlInputBackend::SImpl::EnumerateExistingGamepads()
{
    int Count = 0;
    SDL_JoystickID* GamepadIds = SDL_GetGamepads(&Count);

    if (!GamepadIds)
    {
        // SDL_GetGamepads 返回 NULL 表示没有手柄或出错
        if (Count == 0)
        {
            std::fprintf(stderr, "[SdlInputBackend] 信息: 当前无已连接手柄\n");
        }
        else
        {
            std::fprintf(stderr,
                "[SdlInputBackend] 警告: SDL_GetGamepads 返回空指针: %s\n",
                SDL_GetError());
        }
        return;
    }

    std::fprintf(stderr, "[SdlInputBackend] 信息: 发现 %d 个已连接手柄\n", Count);

    for (int Index = 0; Index < Count; ++Index)
    {
        SDL_JoystickID InstanceId = GamepadIds[Index];

        SDL_Gamepad* Handle = SDL_OpenGamepad(InstanceId);
        if (!Handle)
        {
            std::fprintf(stderr,
                "[SdlInputBackend] 警告: 无法打开手柄 (instance_id: %u): %s\n",
                static_cast<uint32>(InstanceId), SDL_GetError());
            continue;
        }

        SDeviceInfo DeviceInfo = BuildDeviceInfo(InstanceId, Handle);

        SGamepadState State;
        State.Handle = Handle;
        State.Info = DeviceInfo;
        Gamepads[static_cast<uint32>(InstanceId)] = std::move(State);

        AddDeviceInfo(DeviceInfo);

        std::fprintf(stderr,
            "[SdlInputBackend] 信息: 已枚举手柄 \"%s\" (id: \"%s\")\n",
            DeviceInfo.Name.c_str(), DeviceInfo.Id.Value.c_str());
    }

    SDL_free(GamepadIds);
}

void ZSdlInputBackend::SImpl::CleanupSdl()
{
    // 关闭所有已打开的手柄 handle
    for (auto& [Key, State] : Gamepads)
    {
        if (State.Handle)
        {
            SDL_CloseGamepad(State.Handle);
            State.Handle = nullptr;
        }
    }
    Gamepads.clear();

    {
        std::scoped_lock Lock(DeviceMutex);
        Devices.clear();
    }

    // 退出 SDL gamepad 子系统
    if (bSdlInitialized)
    {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        bSdlInitialized = false;
    }
}

void ZSdlInputBackend::SImpl::AddDeviceInfo(const SDeviceInfo& DeviceInfo)
{
    std::scoped_lock Lock(DeviceMutex);
    Devices.push_back(DeviceInfo);
}

void ZSdlInputBackend::SImpl::RemoveDeviceInfo(const SDeviceId& DeviceId)
{
    std::scoped_lock Lock(DeviceMutex);

    auto Iterator = std::find_if(
        Devices.begin(),
        Devices.end(),
        [&DeviceId](const SDeviceInfo& Existing)
        {
            return Existing.Id == DeviceId;
        });

    if (Iterator != Devices.end())
    {
        Devices.erase(Iterator);
    }
}

// ── ZSdlInputBackend 公共接口实现 ──

ZSdlInputBackend::ZSdlInputBackend()
    : Impl(std::make_unique<SImpl>(*this))
{
}

ZSdlInputBackend::~ZSdlInputBackend()
{
    Stop();
}

TResult<void> ZSdlInputBackend::Start()
{
    // 幂等：已运行时直接返回成功
    if (Impl->bRunning.load())
    {
        return TResult<void>::Ok();
    }

    // 初始化 SDL gamepad 子系统（不初始化 VIDEO，窗口由 Qt 管理）
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
    {
        const char* ErrorMessage = SDL_GetError();
        std::fprintf(stderr,
            "[SdlInputBackend] 错误: SDL gamepad 子系统初始化失败: %s\n",
            ErrorMessage ? ErrorMessage : "unknown error");
        return TResult<void>::Err(MakeError(
            EErrorCode::Unknown,
            StdString("SDL gamepad subsystem init failed: ")
                + (ErrorMessage ? ErrorMessage : "unknown")));
    }
    Impl->bSdlInitialized = true;

    // 枚举并打开当前已连接的手柄
    Impl->EnumerateExistingGamepads();

    // 启动 worker 线程
    try
    {
        Impl->WorkerThread = std::jthread(
            [this](std::stop_token StopToken)
            {
                Impl->WorkerMain(StopToken);
            });
    }
    catch (const std::system_error& Exception)
    {
        // worker 创建失败，回滚已打开的 SDL 资源
        std::fprintf(stderr,
            "[SdlInputBackend] 错误: 无法创建输入轮询线程: %s\n",
            Exception.what());
        Impl->CleanupSdl();
        return TResult<void>::Err(MakeError(
            EErrorCode::Unknown,
            StdString("failed to create input worker thread: ") + Exception.what()));
    }

    Impl->bRunning.store(true);

    std::fprintf(stderr, "[SdlInputBackend] 信息: SDL 输入后端已启动\n");
    return TResult<void>::Ok();
}

void ZSdlInputBackend::Stop()
{
    // 幂等：未运行时安全无副作用
    if (!Impl->bRunning.exchange(false))
    {
        return;
    }

    std::fprintf(stderr, "[SdlInputBackend] 信息: 正在停止 SDL 输入后端...\n");

    // 请求 worker 停止并等待退出
    if (Impl->WorkerThread.joinable())
    {
        Impl->WorkerThread.request_stop();
        Impl->WorkerThread.join();
    }

    // worker 已停止，安全清理 SDL 资源
    Impl->CleanupSdl();

    std::fprintf(stderr, "[SdlInputBackend] 信息: SDL 输入后端已停止\n");
}

bool ZSdlInputBackend::IsRunning() const noexcept
{
    return Impl->bRunning.load();
}

TVector<SDeviceInfo> ZSdlInputBackend::ListDevices() const
{
    std::scoped_lock Lock(Impl->DeviceMutex);
    return Impl->Devices;
}

}  // namespace ZeroMapper
