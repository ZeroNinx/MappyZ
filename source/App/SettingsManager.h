// 应用级设置管理器。
// 应用偏好的唯一权威来源，本轮只承载 startMinimized。使用 QSettings::IniFormat
// 持久化到独立的应用配置文件（非 Windows 注册表、非 mapping profile JSON）。
// 归属 Desktop/App 层，只依赖 Qt Core，不涉及 Widgets / QML / Runtime。

#pragma once

#include <memory>

#include <QObject>
#include <QString>

class QSettings;

namespace MappyZ
{

class ZSettingsManager final : public QObject
{
    Q_OBJECT

    // 启动即最小化：控制下一次进程启动时主窗口是否隐藏到托盘。
    // QML 只读该属性并通过 setStartMinimized 修改，不维护第二份状态源。
    Q_PROPERTY(bool startMinimized READ IsStartMinimized NOTIFY startMinimizedChanged)

public:
    // 生产构造：使用 AppConfigLocation 下的 settings.ini 默认路径。
    explicit ZSettingsManager(QObject* Parent = nullptr);

    // 测试构造：注入隔离的 INI 文件路径，不改动全局 QStandardPaths 环境。
    explicit ZSettingsManager(QString FilePath, QObject* Parent = nullptr);

    ~ZSettingsManager() override;

    // 当前启动即最小化偏好；键缺失或非法时安全回退为 true。
    [[nodiscard]] bool IsStartMinimized() const;

    // 写入新的偏好值：
    // - 相同值直接返回 true，不写盘也不发信号；
    // - 写入成功更新缓存并发射一次 startMinimizedChanged，返回 true；
    // - 写入失败保留旧缓存、发射 settingsError，返回 false。
    Q_INVOKABLE bool setStartMinimized(bool bEnabled);

signals:
    // 命名遵循项目 QML 桥接约定（lowerCamelCase signal），与 ZAppController 一致。
    void startMinimizedChanged();
    void settingsError(const QString& Message);

private:
    // 解析生产环境默认设置文件路径（AppConfigLocation/settings.ini）。
    [[nodiscard]] static QString ResolveDefaultSettingsFilePath();

    // 以保存的文件路径 + IniFormat 重建 QSettings 后端。用于构造，以及在写入失败或
    // 读取出错后丢弃未落盘的待写值、清除 QSettings 的持续错误状态，避免内存值与磁盘
    // 不一致或错误状态污染后续读写判定。内部先销毁旧实例再创建新实例：Qt 按文件路径
    // 共享底层 QConfFile，只有旧实例引用计数归零后新实例才能获得干净状态。
    void ResetSettingsBackend();

    // 从持久化读取并校验 startMinimized；缺失/读错误/非法值回退 true。读取出错时会
    // 重建后端清除持续错误状态（非 const），避免残留错误状态导致后续写入全部被误判失败。
    [[nodiscard]] bool ReadStartMinimized();

    // 设置文件路径，重建后端时复用。
    QString SettingsFilePath;

    // 持久化后端：以显式文件路径 + IniFormat 构造，读操作不创建文件。
    std::unique_ptr<QSettings> Settings;

    // 内存权威缓存：构造时初始化，写入成功后刷新。
    bool bStartMinimizedCache = true;
};

}  // namespace MappyZ
