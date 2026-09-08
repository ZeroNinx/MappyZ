// ZForegroundApplicationService 单元测试。
// 全部通过注入 ZFakeForegroundApplicationSource 驱动，不触碰任何平台 API，可在
// offscreen 环境运行。信号用手动 connect 计数（direct connection 同步触发）。

#include <catch2/catch_test_macros.hpp>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

#include "App/ForegroundApplicationService.h"

using namespace MappyZ;

namespace
{

// 构造一条前台窗口信息。
SForegroundWindowInfo MakeInfo(
    const QString& ProcessName, const QString& DisplayName = QString())
{
    SForegroundWindowInfo Info;
    Info.ProcessName = ProcessName;
    Info.DisplayName = DisplayName;
    return Info;
}

// 记录服务广播的每一次前台进程名。
struct SForegroundRecorder
{
    QStringList Names;

    void Attach(ZForegroundApplicationService& Service)
    {
        QObject::connect(&Service,
            &ZForegroundApplicationService::foregroundApplicationChanged,
            [this](QString Name) { Names.append(Name); });
    }
};

}  // namespace

TEST_CASE("Foreground service reports source unavailability", "[App][Foreground]")
{
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    Source->SetAvailable(false);
    ZForegroundApplicationService Service(std::move(Source));

    REQUIRE_FALSE(Service.isAvailable());
}

TEST_CASE("Foreground service start emits current foreground once", "[App][Foreground]")
{
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    Source->SetCurrentForeground(MakeInfo(QStringLiteral("eldenring.exe")));
    auto* SourcePtr = Source.get();

    ZForegroundApplicationService Service(std::move(Source));
    SForegroundRecorder Recorder;
    Recorder.Attach(Service);

    Service.start();

    REQUIRE(SourcePtr->IsStarted());
    REQUIRE(Recorder.Names == QStringList{QStringLiteral("eldenring.exe")});
}

TEST_CASE("Foreground service unavailable source never starts or emits", "[App][Foreground]")
{
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    Source->SetAvailable(false);
    Source->SetCurrentForeground(MakeInfo(QStringLiteral("eldenring.exe")));
    auto* SourcePtr = Source.get();

    ZForegroundApplicationService Service(std::move(Source));
    SForegroundRecorder Recorder;
    Recorder.Attach(Service);

    Service.start();

    REQUIRE_FALSE(SourcePtr->IsStarted());
    REQUIRE(Recorder.Names.isEmpty());
}

TEST_CASE("Foreground service start fails when source load fails", "[App][Foreground]")
{
    // 来源可用但底层装载失败（如 Windows 钩子安装失败）：start 返回 false，
    // 服务保持未启动态，不广播任何事件，后续投递也被丢弃。
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    Source->SetAvailable(true);
    Source->SetStartSucceeds(false);
    Source->SetCurrentForeground(MakeInfo(QStringLiteral("eldenring.exe")));
    auto* SourcePtr = Source.get();

    ZForegroundApplicationService Service(std::move(Source));
    SForegroundRecorder Recorder;
    Recorder.Attach(Service);

    REQUIRE_FALSE(Service.start());
    REQUIRE_FALSE(SourcePtr->IsStarted());
    REQUIRE(Recorder.Names.isEmpty());

    SourcePtr->EmitForeground(MakeInfo(QStringLiteral("game.exe")));
    REQUIRE(Recorder.Names.isEmpty());
}

TEST_CASE("Foreground service debounces repeated process name", "[App][Foreground]")
{
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    auto* SourcePtr = Source.get();
    ZForegroundApplicationService Service(std::move(Source));
    SForegroundRecorder Recorder;
    Recorder.Attach(Service);

    Service.start();

    // 同名不同窗口标题：应只广播一次。
    SourcePtr->EmitForeground(MakeInfo(QStringLiteral("game.exe"), QStringLiteral("Level 1")));
    SourcePtr->EmitForeground(MakeInfo(QStringLiteral("game.exe"), QStringLiteral("Level 2")));
    SourcePtr->EmitForeground(MakeInfo(QStringLiteral("other.exe")));

    REQUIRE(Recorder.Names == QStringList{
        QStringLiteral("game.exe"), QStringLiteral("other.exe")});
}

TEST_CASE("Foreground service emits empty name for unknown foreground", "[App][Foreground]")
{
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    auto* SourcePtr = Source.get();
    ZForegroundApplicationService Service(std::move(Source));
    SForegroundRecorder Recorder;
    Recorder.Attach(Service);

    Service.start();
    SourcePtr->EmitForeground(MakeInfo(QStringLiteral("game.exe")));
    // 前台变为未知（如安全桌面）：广播空串。
    SourcePtr->EmitForeground(SForegroundWindowInfo{});

    REQUIRE(Recorder.Names == QStringList{QStringLiteral("game.exe"), QString()});
}

TEST_CASE("Foreground service stop prevents further emissions", "[App][Foreground]")
{
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    auto* SourcePtr = Source.get();
    ZForegroundApplicationService Service(std::move(Source));
    SForegroundRecorder Recorder;
    Recorder.Attach(Service);

    Service.start();
    Service.stop();
    REQUIRE_FALSE(SourcePtr->IsStarted());

    // 停止后来源不再回调，即使外部尝试投递也不广播。
    SourcePtr->EmitForeground(MakeInfo(QStringLiteral("game.exe")));
    REQUIRE(Recorder.Names.isEmpty());
}

TEST_CASE("Foreground service lists running applications sorted and deduped", "[App][Foreground]")
{
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    Source->SetApplications({
        MakeInfo(QStringLiteral("game.exe"), QStringLiteral("Zeta Window")),
        MakeInfo(QStringLiteral("editor.exe"), QStringLiteral("alpha editor")),
        // 与 game.exe 同名（大小写不敏感）但显示名排序更靠前，去重后保留此项。
        MakeInfo(QStringLiteral("GAME.exe"), QStringLiteral("Beta Window")),
        // 空进程名丢弃。
        MakeInfo(QString(), QStringLiteral("Ghost")),
    });

    ZForegroundApplicationService Service(std::move(Source));
    const QVariantList Apps = Service.runningApplications();

    REQUIRE(Apps.size() == 2);
    // 按 displayName 大小写不敏感排序："alpha editor" < "Beta Window"。
    const QVariantMap First = Apps.at(0).toMap();
    const QVariantMap Second = Apps.at(1).toMap();
    REQUIRE(First.value(QStringLiteral("processName")).toString() == QStringLiteral("editor.exe"));
    REQUIRE(First.value(QStringLiteral("displayName")).toString() == QStringLiteral("alpha editor"));
    // 去重保留排序后首个遇到的（Beta Window 早于 Zeta Window），保留原始大小写。
    REQUIRE(Second.value(QStringLiteral("processName")).toString() == QStringLiteral("GAME.exe"));
    REQUIRE(Second.value(QStringLiteral("displayName")).toString() == QStringLiteral("Beta Window"));
}

TEST_CASE("Foreground service falls back displayName to processName", "[App][Foreground]")
{
    auto Source = std::make_unique<ZFakeForegroundApplicationSource>();
    Source->SetApplications({
        MakeInfo(QStringLiteral("noname.exe")),  // 无窗口标题
    });

    ZForegroundApplicationService Service(std::move(Source));
    const QVariantList Apps = Service.runningApplications();

    REQUIRE(Apps.size() == 1);
    const QVariantMap Entry = Apps.at(0).toMap();
    REQUIRE(Entry.value(QStringLiteral("displayName")).toString() == QStringLiteral("noname.exe"));
}
