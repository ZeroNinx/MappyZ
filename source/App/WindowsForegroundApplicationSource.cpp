// ZWindowsForegroundApplicationSource 实现。
// 使用 WINEVENT_OUTOFCONTEXT 钩子：回调经消息循环投递到安装钩子的 GUI 线程，
// 因此无需额外线程同步。进程名通过 QueryFullProcessImageNameW 获取后取 basename。

#include "App/WindowsForegroundApplicationSource.h"

#include <utility>

// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

#include <QFileInfo>
#include <QString>

namespace MappyZ
{

namespace
{

// 单实例路由指针：WinEvent 回调是 C 风格自由函数，通过此指针找回当前源。
// 应用内只装配一个前台服务，Start 时设置，Stop/析构时清除。
ZWindowsForegroundApplicationSource* GActiveSource = nullptr;

// 从 HWND 读取进程可执行文件 basename。失败返回空串。
QString ProcessNameFromWindow(HWND Window)
{
    if (Window == nullptr)
    {
        return QString();
    }

    DWORD ProcessId = 0;
    GetWindowThreadProcessId(Window, &ProcessId);
    if (ProcessId == 0)
    {
        return QString();
    }

    HANDLE Process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ProcessId);
    if (Process == nullptr)
    {
        return QString();
    }

    wchar_t Buffer[MAX_PATH] = {};
    DWORD Size = static_cast<DWORD>(std::size(Buffer));
    QString FullPath;
    if (QueryFullProcessImageNameW(Process, 0, Buffer, &Size) != 0)
    {
        FullPath = QString::fromWCharArray(Buffer, static_cast<int>(Size));
    }
    CloseHandle(Process);

    if (FullPath.isEmpty())
    {
        return QString();
    }
    // 只取文件名部分，如 "C:\\Games\\EldenRing.exe" -> "EldenRing.exe"。
    return QFileInfo(FullPath).fileName();
}

// 读取窗口标题。
QString WindowTitle(HWND Window)
{
    const int Length = GetWindowTextLengthW(Window);
    if (Length <= 0)
    {
        return QString();
    }
    QString Title(Length + 1, QChar('\0'));
    const int Copied = GetWindowTextW(
        Window, reinterpret_cast<wchar_t*>(Title.data()), Length + 1);
    Title.resize(Copied);
    return Title;
}

// 从 HWND 构造前台窗口信息。进程名为空时返回空 optional。
TOptional<SForegroundWindowInfo> InfoFromWindow(HWND Window)
{
    const QString ProcessName = ProcessNameFromWindow(Window);
    if (ProcessName.isEmpty())
    {
        return TOptional<SForegroundWindowInfo>();
    }

    DWORD ProcessId = 0;
    GetWindowThreadProcessId(Window, &ProcessId);

    SForegroundWindowInfo Info;
    Info.ProcessName = ProcessName;
    Info.DisplayName = WindowTitle(Window);
    Info.ProcessId = static_cast<quint32>(ProcessId);
    return Info;
}

// EnumWindows 回调上下文：收集可见的顶层应用窗口。
struct SEnumContext
{
    TVector<SForegroundWindowInfo>* Out = nullptr;
};

BOOL CALLBACK EnumWindowsProc(HWND Window, LPARAM Param)
{
    auto* Context = reinterpret_cast<SEnumContext*>(Param);
    if (Context == nullptr || Context->Out == nullptr)
    {
        return TRUE;
    }

    // 只收可见、非工具窗口、无 owner 的顶层窗口，且必须有标题。
    if (IsWindowVisible(Window) == FALSE)
    {
        return TRUE;
    }
    if (GetWindow(Window, GW_OWNER) != nullptr)
    {
        return TRUE;
    }
    const LONG_PTR ExStyle = GetWindowLongPtrW(Window, GWL_EXSTYLE);
    if ((ExStyle & WS_EX_TOOLWINDOW) != 0)
    {
        return TRUE;
    }
    if (GetWindowTextLengthW(Window) <= 0)
    {
        return TRUE;
    }

    if (auto Info = InfoFromWindow(Window))
    {
        Context->Out->push_back(std::move(*Info));
    }
    return TRUE;
}

// WinEvent 回调：仅处理前台切换事件，路由到当前源。
void CALLBACK ForegroundWinEventProc(
    HWINEVENTHOOK /*Hook*/, DWORD Event, HWND /*Window*/,
    LONG /*ObjectId*/, LONG /*ChildId*/,
    DWORD /*EventThread*/, DWORD /*EventTime*/)
{
    if (Event == EVENT_SYSTEM_FOREGROUND && GActiveSource != nullptr)
    {
        GActiveSource->DispatchForegroundChange();
    }
}

}  // namespace

ZWindowsForegroundApplicationSource::ZWindowsForegroundApplicationSource() = default;

ZWindowsForegroundApplicationSource::~ZWindowsForegroundApplicationSource()
{
    Stop();
}

bool ZWindowsForegroundApplicationSource::IsAvailable() const
{
    return true;
}

bool ZWindowsForegroundApplicationSource::Start(
    std::function<void(SForegroundWindowInfo)> OnChanged)
{
    if (HookHandle != nullptr)
    {
        return true;
    }

    Callback = std::move(OnChanged);
    GActiveSource = this;

    HWINEVENTHOOK Hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, &ForegroundWinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // SetWinEventHook 失败时返回 NULL。此时回滚路由指针与回调，报告启动失败，
    // 让服务保持未启动态并提示自动切换不可用（不能把失败当成功而静默丢事件）。
    if (Hook == nullptr)
    {
        GActiveSource = nullptr;
        Callback = nullptr;
        return false;
    }

    HookHandle = reinterpret_cast<void*>(Hook);
    return true;
}

void ZWindowsForegroundApplicationSource::Stop()
{
    if (HookHandle != nullptr)
    {
        UnhookWinEvent(reinterpret_cast<HWINEVENTHOOK>(HookHandle));
        HookHandle = nullptr;
    }
    if (GActiveSource == this)
    {
        GActiveSource = nullptr;
    }
    Callback = nullptr;
}

void ZWindowsForegroundApplicationSource::DispatchForegroundChange()
{
    if (!Callback)
    {
        return;
    }
    if (auto Info = InfoFromWindow(GetForegroundWindow()))
    {
        Callback(std::move(*Info));
    }
    else
    {
        // 无法解析前台进程（如安全桌面/无标题窗口）时广播“未知前台”。
        Callback(SForegroundWindowInfo{});
    }
}

TOptional<SForegroundWindowInfo>
ZWindowsForegroundApplicationSource::QueryForeground() const
{
    return InfoFromWindow(GetForegroundWindow());
}

TVector<SForegroundWindowInfo>
ZWindowsForegroundApplicationSource::EnumerateApplications() const
{
    TVector<SForegroundWindowInfo> Result;
    SEnumContext Context;
    Context.Out = &Result;
    EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&Context));
    return Result;
}

}  // namespace MappyZ
