// ZSettingsManager 单元测试。
// 全部使用注入的临时 INI 路径，绝不触碰生产 AppConfigLocation。信号次数用手动
// connect 计数（direct connection 同步触发，无需事件循环），与托盘测试风格一致。

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include "App/SettingsManager.h"

using namespace MappyZ;

namespace
{

// 稳定键名，供 fixture 预置与断言使用。
constexpr auto kKey = "general/startMinimized";

// 在给定 INI 路径预置一个 bool 值（模拟上一次会话已持久化）。
void SeedBool(const QString& IniPath, bool bValue)
{
    QSettings Seed(IniPath, QSettings::IniFormat);
    Seed.setValue(QLatin1String(kKey), bValue);
    Seed.sync();
}

}  // namespace

TEST_CASE("Missing settings file defaults StartMinimized to true", "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());
    const QString IniPath = Dir.path() + QStringLiteral("/settings.ini");

    ZSettingsManager Manager(IniPath);
    REQUIRE(Manager.IsStartMinimized());

    // 首次仅读取默认值不应创建文件。
    REQUIRE_FALSE(QFile::exists(IniPath));
}

TEST_CASE("Setting false persists across reconstruction", "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());
    const QString IniPath = Dir.path() + QStringLiteral("/settings.ini");

    {
        ZSettingsManager Manager(IniPath);
        REQUIRE(Manager.setStartMinimized(false));
        REQUIRE_FALSE(Manager.IsStartMinimized());
    }

    // 重新构造仍读取到 false。
    ZSettingsManager Reloaded(IniPath);
    REQUIRE_FALSE(Reloaded.IsStartMinimized());
}

TEST_CASE("Setting true persists across reconstruction", "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());
    const QString IniPath = Dir.path() + QStringLiteral("/settings.ini");

    // 先落一个 false，再改回 true，确认 true 也被真实写入而非依赖默认值。
    SeedBool(IniPath, false);
    {
        ZSettingsManager Manager(IniPath);
        REQUIRE_FALSE(Manager.IsStartMinimized());
        REQUIRE(Manager.setStartMinimized(true));
        REQUIRE(Manager.IsStartMinimized());
    }

    ZSettingsManager Reloaded(IniPath);
    REQUIRE(Reloaded.IsStartMinimized());
}

TEST_CASE("Setting the same value neither writes nor emits", "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());
    const QString IniPath = Dir.path() + QStringLiteral("/settings.ini");

    ZSettingsManager Manager(IniPath);  // 缺失键，缓存默认 true

    int ChangedCount = 0;
    QObject::connect(&Manager, &ZSettingsManager::startMinimizedChanged, &Manager,
        [&ChangedCount]() { ++ChangedCount; });

    // 设置与当前相同的值：成功返回、不发信号、且不创建文件。
    REQUIRE(Manager.setStartMinimized(true));
    REQUIRE(ChangedCount == 0);
    REQUIRE_FALSE(QFile::exists(IniPath));
}

TEST_CASE("Successful change emits once and slot reads the new value",
    "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());
    const QString IniPath = Dir.path() + QStringLiteral("/settings.ini");

    ZSettingsManager Manager(IniPath);  // 缓存默认 true

    int ChangedCount = 0;
    bool ValueInSlot = true;
    QObject::connect(&Manager, &ZSettingsManager::startMinimizedChanged, &Manager,
        [&ChangedCount, &ValueInSlot, &Manager]()
        {
            ++ChangedCount;
            ValueInSlot = Manager.IsStartMinimized();
        });

    REQUIRE(Manager.setStartMinimized(false));
    REQUIRE(ChangedCount == 1);
    REQUIRE_FALSE(ValueInSlot);  // 槽内读取到的是新值
}

TEST_CASE("Write failure returns false, keeps cache, emits settingsError",
    "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());

    // 用一个普通文件充当父目录，使 QSettings 无法在其下创建 INI，制造写入失败。
    const QString Blocker = Dir.path() + QStringLiteral("/blocker");
    {
        QFile File(Blocker);
        REQUIRE(File.open(QIODevice::WriteOnly));
        File.close();
    }
    const QString BadIni = Blocker + QStringLiteral("/settings.ini");

    ZSettingsManager Manager(BadIni);
    // 无法读取，缺失键缓存默认 true。
    REQUIRE(Manager.IsStartMinimized());

    int ErrorCount = 0;
    int ChangedCount = 0;
    QObject::connect(&Manager, &ZSettingsManager::settingsError, &Manager,
        [&ErrorCount](const QString&) { ++ErrorCount; });
    QObject::connect(&Manager, &ZSettingsManager::startMinimizedChanged, &Manager,
        [&ChangedCount]() { ++ChangedCount; });

    // 值变化触发写入，写入失败：返回 false、缓存保持旧值 true、不发 changed、发一次 error。
    REQUIRE_FALSE(Manager.setStartMinimized(false));
    REQUIRE(Manager.IsStartMinimized());
    REQUIRE(ChangedCount == 0);
    REQUIRE(ErrorCount == 1);
}

TEST_CASE("Changing start minimized preserves unknown keys", "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());
    const QString IniPath = Dir.path() + QStringLiteral("/settings.ini");

    // 预置一个未知键与 startMinimized。
    {
        QSettings Seed(IniPath, QSettings::IniFormat);
        Seed.setValue(QLatin1String(kKey), true);
        Seed.setValue(QStringLiteral("general/unknownFuture"),
            QStringLiteral("keep-me"));
        Seed.sync();
    }

    {
        ZSettingsManager Manager(IniPath);
        REQUIRE(Manager.setStartMinimized(false));
    }

    // 修改已知键后未知键仍保留。
    QSettings Check(IniPath, QSettings::IniFormat);
    REQUIRE(Check.value(QStringLiteral("general/unknownFuture")).toString()
        == QStringLiteral("keep-me"));
    REQUIRE_FALSE(Check.value(QLatin1String(kKey)).toBool());
}

TEST_CASE("Two managers with different paths are independent", "[App][Settings]")
{
    QTemporaryDir DirA;
    QTemporaryDir DirB;
    REQUIRE(DirA.isValid());
    REQUIRE(DirB.isValid());
    const QString IniA = DirA.path() + QStringLiteral("/settings.ini");
    const QString IniB = DirB.path() + QStringLiteral("/settings.ini");

    ZSettingsManager ManagerA(IniA);
    ZSettingsManager ManagerB(IniB);

    REQUIRE(ManagerA.setStartMinimized(false));

    // A 的修改不影响 B。
    REQUIRE_FALSE(ManagerA.IsStartMinimized());
    REQUIRE(ManagerB.IsStartMinimized());
}

TEST_CASE("Invalid persisted value falls back to true", "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());
    const QString IniPath = Dir.path() + QStringLiteral("/settings.ini");

    // 写入一个非法（非 bool 记号）值。
    {
        QSettings Seed(IniPath, QSettings::IniFormat);
        Seed.setValue(QLatin1String(kKey), QStringLiteral("not-a-bool"));
        Seed.sync();
    }

    // 非法值安全回退为 true（并在内部经 qWarning 告警一次）。
    ZSettingsManager Manager(IniPath);
    REQUIRE(Manager.IsStartMinimized());
}

TEST_CASE("Write failure does not persist stale value on later sync",
    "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());

    // 用普通文件充当父目录，使 startMinimized 的 INI 无法在其下创建，制造写入失败。
    const QString Blocker = Dir.path() + QStringLiteral("/blocker");
    {
        QFile File(Blocker);
        REQUIRE(File.open(QIODevice::WriteOnly));
        File.close();
    }
    const QString BadIni = Blocker + QStringLiteral("/settings.ini");

    {
        ZSettingsManager Manager(BadIni);
        REQUIRE(Manager.IsStartMinimized());  // 缺失键默认 true

        // 写入失败：返回 false、缓存保持 true。
        REQUIRE_FALSE(Manager.setStartMinimized(false));
        REQUIRE(Manager.IsStartMinimized());
        // 析构在此发生：内部后端已重建，不应把未落盘的 false 再次写盘。
    }

    // 删除 blocker 后目录可创建；重新构造读取应仍是默认 true，
    // 证明失败的写入没有残留待写值污染持久化状态。
    REQUIRE(QFile::remove(Blocker));
    ZSettingsManager Reloaded(BadIni);
    REQUIRE(Reloaded.IsStartMinimized());
}

TEST_CASE("Unreadable settings source falls back to true without treating as first run",
    "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());

    // 让 INI 路径指向一个目录：QSettings 读取时 status() 报错，contains() 返回 false。
    // 此时不能被当作“首次运行键不存在”而静默处理，应回退 true（内部经 qWarning 告警）。
    const QString DirAsIni = Dir.path() + QStringLiteral("/as_dir");
    REQUIRE(QDir(Dir.path()).mkdir(QStringLiteral("as_dir")));

    ZSettingsManager Manager(DirAsIni);
    REQUIRE(Manager.IsStartMinimized());
}

TEST_CASE("Same manager recovers and persists once environment heals after write failure",
    "[App][Settings]")
{
    QTemporaryDir Dir;
    REQUIRE(Dir.isValid());

    // 用普通文件充当父目录，使 INI 无法在其下创建，制造第一次写入失败。
    const QString Blocker = Dir.path() + QStringLiteral("/blocker");
    {
        QFile File(Blocker);
        REQUIRE(File.open(QIODevice::WriteOnly));
        File.close();
    }
    const QString Ini = Blocker + QStringLiteral("/settings.ini");

    ZSettingsManager Manager(Ini);
    REQUIRE(Manager.IsStartMinimized());  // 缺失键默认 true

    int ErrorCount = 0;
    int ChangedCount = 0;
    QObject::connect(&Manager, &ZSettingsManager::settingsError, &Manager,
        [&ErrorCount](const QString&) { ++ErrorCount; });
    QObject::connect(&Manager, &ZSettingsManager::startMinimizedChanged, &Manager,
        [&ChangedCount]() { ++ChangedCount; });

    // 第一次写入失败：返回 false、缓存保持 true、发一次 error、不发 changed。
    REQUIRE_FALSE(Manager.setStartMinimized(false));
    REQUIRE(Manager.IsStartMinimized());
    REQUIRE(ErrorCount == 1);
    REQUIRE(ChangedCount == 0);

    // 环境恢复：移除 blocker，使父目录可创建 INI。
    REQUIRE(QFile::remove(Blocker));

    // 同一个 manager 重试：此前失败没有残留待写值或错误状态污染后端，本次应真正成功。
    // 若 ResetSettingsBackend 未先销毁旧实例、旧的错误状态被继承，这里会仍然返回 false。
    REQUIRE(Manager.setStartMinimized(false));
    REQUIRE_FALSE(Manager.IsStartMinimized());
    REQUIRE(ErrorCount == 1);   // 无新的错误
    REQUIRE(ChangedCount == 1); // 成功变更发一次 changed

    // 重新构造读取应看到已落盘的 false，证明磁盘值与 UI 值一致，重启不会突变。
    ZSettingsManager Reloaded(Ini);
    REQUIRE_FALSE(Reloaded.IsStartMinimized());
}
