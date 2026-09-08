// ZSettingsManager 实现。
// 以显式文件路径 + IniFormat 构造 QSettings，读取时不创建文件；写入后 sync 并检查
// status，失败经 settingsError 上报，成功才刷新内存缓存并发出一次 changed 信号。

#include "App/SettingsManager.h"

#include <utility>

#include <QDebug>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>

namespace MappyZ
{

namespace
{
// 稳定键名，供读写与测试共同引用。
constexpr auto kStartMinimizedKey = "general/startMinimized";
}  // namespace

QString ZSettingsManager::ResolveDefaultSettingsFilePath()
{
    // 刻意使用 AppConfigLocation 而非 profile 的 AppDataLocation：应用偏好属于配置，
    // 映射 profile 属于用户数据。不设置 organizationName/applicationName，沿用默认
    // 应用名，避免间接迁移现有 profile 与设置目录。
    const QString Dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return Dir + QStringLiteral("/settings.ini");
}

ZSettingsManager::ZSettingsManager(QObject* Parent)
    : ZSettingsManager(ResolveDefaultSettingsFilePath(), Parent)
{
}

ZSettingsManager::ZSettingsManager(QString FilePath, QObject* Parent)
    : QObject(Parent)
    , SettingsFilePath(std::move(FilePath))
{
    ResetSettingsBackend();
    // 构造即读取一次并缓存；此后缓存是权威内存值，写入成功后同步刷新。
    bStartMinimizedCache = ReadStartMinimized();
}

ZSettingsManager::~ZSettingsManager() = default;

void ZSettingsManager::ResetSettingsBackend()
{
    // 以显式文件路径重建后端：丢弃任何未落盘的待写值，并清除 QSettings 的持续错误
    // 状态（status() 一旦非 NoError 会保留，污染后续读写判定）。
    //
    // 必须先销毁旧实例再创建新实例：Qt 按文件路径共享底层 QConfFile 缓存，只要旧实例
    // 还存活（引用计数未归零），同路径的新实例会继承旧的待写值与错误状态。若写成
    // Settings = std::make_unique<...>()，右侧新实例会在旧实例析构前构造，无法拿到
    // 干净状态。这里先 reset() 归零引用计数强制刷新缓存，再构造新实例。
    Settings.reset();
    Settings = std::make_unique<QSettings>(SettingsFilePath, QSettings::IniFormat);
}

bool ZSettingsManager::IsStartMinimized() const
{
    return bStartMinimizedCache;
}

bool ZSettingsManager::setStartMinimized(bool bEnabled)
{
    // 相同值：不写盘、不发信号。也保证首次仅读取默认值时不强制创建文件。
    if (bEnabled == bStartMinimizedCache)
    {
        return true;
    }

    Settings->setValue(QLatin1String(kStartMinimizedKey), bEnabled);
    Settings->sync();

    if (Settings->status() != QSettings::NoError)
    {
        // 写入失败：保留旧缓存，经 Qt 日志体系与 settingsError 上报，
        // 不使用仅控制台可见的 fprintf(stderr) 作为唯一诊断通道。
        const QString Message =
            QStringLiteral("无法保存设置 startMinimized：写入配置文件失败");
        qWarning().noquote() << QStringLiteral("[MappyZ]") << Message;

        // 重建后端丢弃这次未落盘的新值并清除持续错误状态，使内存缓存与磁盘保持
        // 一致，避免后续重试或析构同步时把陈旧的待写值再次落盘。
        ResetSettingsBackend();

        emit settingsError(Message);
        return false;
    }

    bStartMinimizedCache = bEnabled;
    emit startMinimizedChanged();
    return true;
}

bool ZSettingsManager::ReadStartMinimized()
{
    const QString Key = QLatin1String(kStartMinimizedKey);
    const bool bHasKey = Settings->contains(Key);

    // contains() 之后立即检查状态：配置文件不可读或格式损坏时 status() 非 NoError，
    // 此时 contains() 的 false 不代表“首次运行键不存在”，不能静默按默认处理。
    if (Settings->status() != QSettings::NoError)
    {
        qWarning().noquote()
            << QStringLiteral("[MappyZ] 读取设置文件失败，回退 startMinimized 为 true:")
            << SettingsFilePath;

        // 重建后端清除持续错误状态：否则残留错误会使之后所有写入的 sync() 状态检查
        // 都被误判为失败，环境恢复后也无法再成功写入。
        ResetSettingsBackend();
        return true;
    }

    if (!bHasKey)
    {
        // 键不存在是首次运行的正常路径：默认最小化启动，不视为错误、不告警。
        return true;
    }

    // INI 中值以字符串保存；显式校验合法 bool 记号，非法值安全回退并告警一次，
    // 避免 QVariant::toBool 把非法串静默当成 false。
    const QString Token = Settings->value(Key).toString().trimmed().toLower();
    if (Token == QLatin1String("true") || Token == QLatin1String("1"))
    {
        return true;
    }
    if (Token == QLatin1String("false") || Token == QLatin1String("0"))
    {
        return false;
    }

    qWarning().noquote()
        << QStringLiteral("[MappyZ] 设置 general/startMinimized 值非法，回退为 true:")
        << Token;
    return true;
}

}  // namespace MappyZ
