// 启动窗口可见性策略。
// 纯函数，无 QObject / Qt Widgets 依赖，与窗口事件处理（ZWindowLifecycleController）
// 是两个独立职责：本函数只回答“进程启动时根窗口该 show 还是 hide”。
// 归属 MappyZDesktopCore，可由测试直接调用。

#pragma once

namespace MappyZ
{

// 启动时是否显示主窗口。
// 仅当托盘可用且用户选择“启动即最小化”时隐藏；其余组合一律显示，
// 确保托盘不可用时应用始终有可见入口，不会因该偏好而无法访问。
[[nodiscard]] inline bool ShouldShowMainWindow(bool bTrayAvailable, bool bStartMinimized)
{
    return !(bTrayAvailable && bStartMinimized);
}

}  // namespace MappyZ
