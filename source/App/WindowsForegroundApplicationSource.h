// Windows 前台应用来源：基于 SetWinEventHook(EVENT_SYSTEM_FOREGROUND) 的事件驱动
// 实现，不轮询。前台窗口变化时读取其进程可执行文件 basename 与窗口标题广播出去。
// 仅在 Windows 平台编译（MAPPYZ_HAS_WINDOWS_FOREGROUND）。
//
// 头文件不包含 <windows.h>：钩子句柄以 void* 保存，Win32 细节封装在 .cpp。

#pragma once

#include <functional>

#include "App/ForegroundApplicationService.h"

namespace MappyZ
{

class ZWindowsForegroundApplicationSource final
    : public IForegroundApplicationSource
{
public:
    ZWindowsForegroundApplicationSource();
    ~ZWindowsForegroundApplicationSource() override;

    NODISCARD bool IsAvailable() const override;
    NODISCARD bool Start(std::function<void(SForegroundWindowInfo)> OnChanged) override;
    void Stop() override;
    NODISCARD TOptional<SForegroundWindowInfo> QueryForeground() const override;
    NODISCARD TVector<SForegroundWindowInfo> EnumerateApplications() const override;

    // 供 WinEvent 回调路由：把一次前台变化投递给已注册回调。
    void DispatchForegroundChange();

private:
    void* HookHandle = nullptr;  // HWINEVENTHOOK
    std::function<void(SForegroundWindowInfo)> Callback;
};

}  // namespace MappyZ
