// 前台应用监听服务。
// 以事件驱动方式观察系统前台窗口变化，并把当前前台可执行文件名对外广播；
// 同时提供当前可见顶层应用列表，供进程选择界面回填。
//
// 服务本体只依赖 Qt Core，平台细节封装在可注入的 IForegroundApplicationSource
// 后端：真实 Windows 实现挂 SetWinEventHook（事件驱动，非轮询），测试用 fake 直接
// 投递，非 Windows 平台用不可用源。这样服务可在 offscreen 环境下用 fake 做自动化测试。

#pragma once

#include <functional>
#include <memory>

#include <QObject>
#include <QString>
#include <QVariantList>

#include "Core/ProjectCore.h"

namespace MappyZ
{

// 平台无关的前台窗口信息（原始，未规范化）。规范化由消费方（AppController）统一完成。
struct SForegroundWindowInfo
{
    QString ProcessName;   // 可执行文件 basename，如 "eldenring.exe"
    QString DisplayName;   // 窗口标题或进程名，仅用于展示
    quint32 ProcessId = 0;
};

// 前台应用来源抽象：把“如何得知前台变化 / 如何枚举应用”与服务解耦。
// Start 传入的回调在 GUI 线程被调用（Windows 使用 WINEVENT_OUTOFCONTEXT，
// 回调经消息循环投递到安装钩子的线程）。
class IForegroundApplicationSource
{
public:
    virtual ~IForegroundApplicationSource() = default;

    // 该源在当前平台是否可用。非 Windows 平台或初始化失败返回 false。
    NODISCARD virtual bool IsAvailable() const = 0;

    // 开始监听前台变化。OnChanged 每次前台窗口切换时被调用一次。
    // 返回是否成功装载底层监听（如 Windows 钩子安装失败返回 false，此时不产生事件）。
    NODISCARD virtual bool Start(std::function<void(SForegroundWindowInfo)> OnChanged) = 0;

    // 停止监听并释放平台资源。可重复调用。
    virtual void Stop() = 0;

    // 查询当前前台窗口，用于启动时立即求值一次。无前台或不可用时返回空。
    NODISCARD virtual TOptional<SForegroundWindowInfo> QueryForeground() const = 0;

    // 枚举当前可见顶层应用，供进程选择器展示。
    NODISCARD virtual TVector<SForegroundWindowInfo> EnumerateApplications() const = 0;
};

class ZForegroundApplicationService final : public QObject
{
    Q_OBJECT

public:
    // 注入平台来源。服务拥有该来源的所有权。
    explicit ZForegroundApplicationService(
        std::unique_ptr<IForegroundApplicationSource> Source,
        QObject* Parent = nullptr);

    ~ZForegroundApplicationService() override;

    // 来源是否可用（不可用时 start 不会产生任何事件）。
    NODISCARD bool isAvailable() const;

    // 开始监听：注册来源回调，并立即用当前前台触发一次 foregroundApplicationChanged。
    // 返回是否成功启动；来源不可用时返回 false 且不产生任何事件（调用方据此提示一次）。
    bool start();

    // 停止监听。
    void stop();

    // 供进程选择器：当前可见顶层应用列表，每项含 processName / displayName 两个键。
    // 同一 processName 只出现一次（保留首个遇到的显示名），按显示名排序。
    Q_INVOKABLE QVariantList runningApplications() const;

signals:
    // 前台应用切换。processName 为原始 basename（未规范化）；空串表示前台未知。
    void foregroundApplicationChanged(QString processName);

private:
    // 来源回调入口：去抖后按需发信号。
    void HandleSourceChange(const SForegroundWindowInfo& Info);

    std::unique_ptr<IForegroundApplicationSource> Source;

    // 上一次广播的进程名，用于去抖（同名不重复发）。
    QString LastProcessName;

    bool bStarted = false;
};

// 构造当前平台默认来源：Windows 上返回 SetWinEventHook 实现，其它平台返回不可用源。
NODISCARD std::unique_ptr<IForegroundApplicationSource>
MakeDefaultForegroundApplicationSource();

// 永远不可用的空来源，供非 Windows 平台与显式禁用场景使用。
class ZUnavailableForegroundApplicationSource final
    : public IForegroundApplicationSource
{
public:
    NODISCARD bool IsAvailable() const override { return false; }
    bool Start(std::function<void(SForegroundWindowInfo)>) override { return false; }
    void Stop() override {}
    NODISCARD TOptional<SForegroundWindowInfo> QueryForeground() const override
    {
        return TOptional<SForegroundWindowInfo>();
    }
    NODISCARD TVector<SForegroundWindowInfo> EnumerateApplications() const override
    {
        return {};
    }
};

// 可编程 fake 来源，供自动化测试与 offscreen 冒烟。测试可直接投递前台变化、
// 预置枚举列表并控制可用性；不触碰任何平台 API。
class ZFakeForegroundApplicationSource final
    : public IForegroundApplicationSource
{
public:
    NODISCARD bool IsAvailable() const override { return bAvailable; }
    void SetAvailable(bool bValue) { bAvailable = bValue; }

    // 模拟底层装载成功/失败：为 false 时 Start 返回 false 且不进入已启动态。
    void SetStartSucceeds(bool bValue) { bStartSucceeds = bValue; }

    bool Start(std::function<void(SForegroundWindowInfo)> OnChanged) override
    {
        if (!bStartSucceeds)
        {
            return false;
        }
        Callback = std::move(OnChanged);
        bStarted = true;
        return true;
    }

    void Stop() override
    {
        bStarted = false;
        Callback = nullptr;
    }

    NODISCARD TOptional<SForegroundWindowInfo> QueryForeground() const override
    {
        return CurrentForeground;
    }

    NODISCARD TVector<SForegroundWindowInfo> EnumerateApplications() const override
    {
        return Applications;
    }

    // ── 测试驱动接口 ──

    // 设置“当前前台”，供 start 时 QueryForeground 使用。
    void SetCurrentForeground(TOptional<SForegroundWindowInfo> Info)
    {
        CurrentForeground = std::move(Info);
    }

    // 预置枚举列表。
    void SetApplications(TVector<SForegroundWindowInfo> List)
    {
        Applications = std::move(List);
    }

    // 模拟一次前台切换事件；已 Start 时触发回调。
    void EmitForeground(SForegroundWindowInfo Info)
    {
        CurrentForeground = Info;
        if (bStarted && Callback)
        {
            Callback(std::move(Info));
        }
    }

    NODISCARD bool IsStarted() const { return bStarted; }

private:
    std::function<void(SForegroundWindowInfo)> Callback;
    TOptional<SForegroundWindowInfo> CurrentForeground;
    TVector<SForegroundWindowInfo> Applications;
    bool bAvailable = true;
    bool bStarted = false;
    bool bStartSucceeds = true;
};

}  // namespace MappyZ
