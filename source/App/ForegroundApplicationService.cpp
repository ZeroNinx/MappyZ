// ZForegroundApplicationService 实现。
// 服务本体只做去抖与信号转发，平台细节全部在注入的来源里。

#include "App/ForegroundApplicationService.h"

#include <algorithm>
#include <utility>

#include <QSet>
#include <QVariantMap>

#if defined(MAPPYZ_HAS_WINDOWS_FOREGROUND)
#include "App/WindowsForegroundApplicationSource.h"
#endif

namespace MappyZ
{

ZForegroundApplicationService::ZForegroundApplicationService(
    std::unique_ptr<IForegroundApplicationSource> InSource,
    QObject* Parent)
    : QObject(Parent)
    , Source(std::move(InSource))
{
}

ZForegroundApplicationService::~ZForegroundApplicationService()
{
    if (bStarted && Source)
    {
        Source->Stop();
    }
}

bool ZForegroundApplicationService::isAvailable() const
{
    return Source && Source->IsAvailable();
}

bool ZForegroundApplicationService::start()
{
    if (bStarted)
    {
        return true;
    }
    if (!Source || !Source->IsAvailable())
    {
        return false;
    }

    // 仅在底层来源成功装载后进入已启动态；装载失败（如钩子安装失败）时保持未启动，
    // 由调用方据返回值提示自动切换不可用。
    if (!Source->Start([this](SForegroundWindowInfo Info) {
            HandleSourceChange(Info);
        }))
    {
        return false;
    }
    bStarted = true;

    // 启动即用当前前台求值一次，保证冷启动时立即应用正确配置。
    if (auto Current = Source->QueryForeground())
    {
        HandleSourceChange(*Current);
    }
    return true;
}

void ZForegroundApplicationService::stop()
{
    if (!bStarted || !Source)
    {
        return;
    }
    bStarted = false;
    Source->Stop();
}

void ZForegroundApplicationService::HandleSourceChange(
    const SForegroundWindowInfo& Info)
{
    // 同一进程名不重复广播，避免同应用不同窗口切换造成的抖动。
    if (Info.ProcessName == LastProcessName)
    {
        return;
    }
    LastProcessName = Info.ProcessName;
    emit foregroundApplicationChanged(Info.ProcessName);
}

QVariantList ZForegroundApplicationService::runningApplications() const
{
    QVariantList Result;
    if (!Source)
    {
        return Result;
    }

    auto Apps = Source->EnumerateApplications();

    // 按显示名排序，稳定展示顺序。
    std::sort(Apps.begin(), Apps.end(),
        [](const SForegroundWindowInfo& A, const SForegroundWindowInfo& B) {
            return A.DisplayName.compare(B.DisplayName, Qt::CaseInsensitive) < 0;
        });

    // 同一 processName 只保留首个（大小写不敏感去重）。
    QSet<QString> Seen;
    for (const auto& App : Apps)
    {
        const QString Key = App.ProcessName.toLower();
        if (App.ProcessName.isEmpty() || Seen.contains(Key))
        {
            continue;
        }
        Seen.insert(Key);

        QVariantMap Entry;
        Entry.insert(QStringLiteral("processName"), App.ProcessName);
        Entry.insert(QStringLiteral("displayName"),
            App.DisplayName.isEmpty() ? App.ProcessName : App.DisplayName);
        Result.append(Entry);
    }
    return Result;
}

std::unique_ptr<IForegroundApplicationSource>
MakeDefaultForegroundApplicationSource()
{
#if defined(MAPPYZ_HAS_WINDOWS_FOREGROUND)
    return std::make_unique<ZWindowsForegroundApplicationSource>();
#else
    return std::make_unique<ZUnavailableForegroundApplicationSource>();
#endif
}

}  // namespace MappyZ
