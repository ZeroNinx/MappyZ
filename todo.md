# TODO: Desktop Tray Lifecycle

## Goal

实现桌面端托盘生命周期：应用启动后默认隐藏到系统托盘；用户关闭主窗口或最小化主窗口时只隐藏窗口，不停止 Runtime、不退出进程；托盘右键菜单本轮只提供 `Exit`，托盘图标和应用窗口统一使用根目录 `icon.ico`。

本轮只处理 Windows 桌面应用的窗口与托盘生命周期，不扩展映射、Profile 或其他 UI 功能。

本文件是本轮唯一实施范围，替代已删除的 `todo_ui.md`；旧 UI TODO 不恢复。

## Scope

包含：

- [ ] 启动后 Runtime 正常初始化并运行，但主窗口默认不显示。
- [ ] 最小化主窗口时隐藏到托盘，进程和映射保持运行。
- [ ] 点击窗口关闭按钮时隐藏到托盘，进程和映射保持运行。
- [ ] 点击托盘图标恢复并激活主窗口。
- [ ] 托盘右键菜单仅包含 `Exit`。
- [ ] `Exit` 执行统一的 Runtime 清理后真正退出进程。
- [ ] 托盘、窗口和 Windows 可执行文件使用 `icon.ico`。
- [ ] 系统托盘不可用时自动显示主窗口，避免应用不可访问。

不包含：

- 开机启动。
- 托盘通知、气泡消息和运行状态动态图标。
- 托盘菜单中的暂停映射、Profile 切换或设备列表。
- 用户可配置的“关闭时退出”选项。
- macOS/Linux 托盘行为适配和平台专属菜单。

## Priority 0: Lifecycle Contract

- [ ] 明确 `Close`、`Minimize` 与 `Exit` 是三个不同操作：
  - [ ] `Close`：忽略关闭事件并隐藏主窗口。
  - [ ] `Minimize`：隐藏主窗口，不关闭、不销毁 QML Window。
  - [ ] `Exit`：停止 pump、停止 Runtime、隐藏托盘图标并退出进程。
- [ ] 主窗口隐藏期间不调用 `stopPumpTimer()` 或 `stopRuntime()`。
- [ ] Runtime 生命周期不再依赖 QML `Window.onClosing`；退出清理由 C++ 的统一退出路径负责。
- [ ] 清理逻辑必须幂等，托盘退出、系统退出或初始化失败后的退出不能造成重复析构问题。
- [ ] `quitOnLastWindowClosed` 根据托盘能力设置：托盘可用时为 `false`，托盘不可用时为 `true`。
- [ ] 只有托盘可用且托盘图标成功建立时才采用启动隐藏策略。
- [ ] `QSystemTrayIcon::isSystemTrayAvailable() == false` 时显示主窗口并保持普通窗口生命周期，日志输出明确警告。
- [ ] 将“窗口事件策略”与“系统托盘实现”分离：offscreen 自动化测试不创建、不探测真实托盘。

## Priority 1: Window Lifecycle And Tray Adapter

本模块拆成两个职责单一的对象，避免 `QSystemTrayIcon`、offscreen 测试和窗口策略相互耦合。

### 1. Window Lifecycle Controller

新增 `ZWindowLifecycleController final : public QObject`，只依赖 Qt Core/Gui，不包含 `QSystemTrayIcon`、`QMenu` 或 QAction。

职责：

- [ ] 使用 `QPointer<QWindow>` 保存主窗口，不拥有 QML Window，并在窗口销毁后自动失效。
- [ ] 接收由装配层设置的 `bTrayAvailable`，不自行探测系统托盘。
- [ ] 在主窗口上安装 `eventFilter`：
  - [ ] 托盘可用且未真正退出时，收到 `QEvent::Close` 后 `ignore()`、隐藏窗口并返回 `true`，阻止事件继续进入 `QQuickWindow`/QML。
  - [ ] 托盘不可用时不拦截 Close，保持普通窗口关闭语义。
  - [ ] 收到最小化状态变化时使用 queued 调用隐藏窗口，避免在状态事件处理中直接修改窗口状态导致重入。
  - [ ] 应用正在真正退出时不再拦截关闭事件。
- [ ] 提供 `BeginExit()`，在调用 `QCoreApplication::quit()` 前关闭事件拦截语义。
- [ ] 提供 `RestoreWindow()`：清除 minimized 状态、调用 `show()`、`raise()` 和 `requestActivate()`。
- [ ] 重复 Attach 时先从旧窗口移除 event filter，避免悬空监听和重复处理。
- [ ] 析构时从仍存活的窗口移除 event filter；窗口销毁后内部指针自动失效。

建议接口：

```cpp
class ZWindowLifecycleController final : public QObject
{
    Q_OBJECT

public:
    explicit ZWindowLifecycleController(QObject* Parent = nullptr);

    void AttachWindow(QWindow* Window);
    void SetTrayAvailable(bool bAvailable);
    void BeginExit();
    void RestoreWindow();

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;
};
```

### 2. System Tray Adapter

新增 `ZSystemTrayController final : public QObject`，依赖 Qt Widgets，只负责真实托盘资源和用户命令。

职责：

- [ ] 持有 `QSystemTrayIcon`、`QMenu` 和唯一的 `Exit` QAction，保证对象生命周期覆盖整个事件循环。
- [ ] `IsAvailable()` 封装 `QSystemTrayIcon::isSystemTrayAvailable()`；该结果由 `Main.cpp` 传给窗口生命周期控制器。
- [ ] 托盘图标单击或双击时发出 `RestoreRequested()`；Context 激活只打开系统菜单，不额外切换窗口状态。
- [ ] `Exit` QAction 只发出语义明确的 `ExitRequested()`，不直接了解 `ZAppController`。
- [ ] 提供 `Show()` / `Hide()` 或等价生命周期接口，确保退出前能主动隐藏托盘图标。
- [ ] 不把 Runtime、Profile 或映射逻辑引入托盘控制器。

建议接口：

```cpp
class ZSystemTrayController final : public QObject
{
    Q_OBJECT

public:
    explicit ZSystemTrayController(const QIcon& Icon, QObject* Parent = nullptr);

    bool IsAvailable() const;
    void Show();
    void Hide();

signals:
    void RestoreRequested();
    void ExitRequested();
};
```

## Priority 2: Application Entry Integration

修改 `source/App/Main.cpp`：

- [ ] 将 `QGuiApplication` 改为 `QApplication`，并在 CMake 中链接 `Qt6::Widgets`。
- [ ] 完成托盘可用性判断后调用 `setQuitOnLastWindowClosed(!bTrayAvailable)`：托盘模式下最后窗口关闭不退出，回退模式下关闭主窗口正常退出。
- [ ] 从 Qt resource 加载 `:/assets/icon.ico`，调用 `QApplication::setWindowIcon()`。
- [ ] 创建 `ZSystemTrayController`，使用同一个 `QIcon` 创建托盘图标。
- [ ] QML 加载成功后安全取得根对象并验证它是 `QWindow`；验证失败时输出错误并退出。
- [ ] 创建 `ZWindowLifecycleController`，将根窗口和托盘可用状态传给它。
- [ ] `ZSystemTrayController::RestoreRequested()` 连接到 `ZWindowLifecycleController::RestoreWindow()`。
- [ ] 托盘可用时显示托盘图标并保持主窗口隐藏。
- [ ] 托盘不可用时直接显示主窗口，不进入不可恢复的隐藏状态。
- [ ] `ExitRequested()` 先调用 `WindowLifecycleController.BeginExit()`，再调用 `QCoreApplication::quit()`。
- [ ] 在 `aboutToQuit` 中按顺序执行：
  - [ ] 隐藏托盘图标。
  - [ ] `AppController.stopPumpTimer()`。
  - [ ] `AppController.stopRuntime()`。
- [ ] 保持 `objectCreationFailed` 的失败退出路径有效。

命名澄清：

- [ ] 本节的 `AppController` 明确指 `MappyZ::ZAppController` QML 桥对象。
- [ ] `ZAppController` 继续委托 `ZApplicationBootstrap` 管理 Runtime；托盘模块不直接持有或调用 `ZApplicationBootstrap`。

所有权与析构顺序：

- [ ] `QApplication` 最先构造、最后析构。
- [ ] `ZAppController` 生命周期长于 QML engine 和托盘退出回调。
- [ ] 托盘控制器及其菜单/action 在事件循环结束前保持存活。
- [ ] `QQmlApplicationEngine` 销毁前不得留下指向根窗口的活动回调。

## Priority 3: QML Window Contract

修改 `ui/Main.qml`：

- [ ] 将主 `Window.visible` 默认值改为 `false`，避免启动时先闪现窗口再隐藏。
- [ ] 完整删除当前 `Window.onClosing` handler，不保留空 handler，也不在 QML 中调用 `stopPumpTimer()` / `stopRuntime()`。
- [ ] 不在 QML 中重复实现关闭拦截、最小化监听或托盘菜单。
- [ ] 保留 `Component.onCompleted` 的 initialize -> load profile -> start runtime -> start pump 流程，使隐藏启动仍能正常工作。
- [ ] QML 初始化失败时继续通过现有错误路径反馈，不因窗口隐藏而崩溃。
- [ ] 自动化测试和 Windows 手动验收共同确认：C++ event filter 返回 `true` 后，关闭事件不会继续触发 QML Window 的关闭流程。

## Priority 4: Icon Packaging

- [ ] 使用 `qt_add_resources` 将根目录 `icon.ico` 以 `:/assets/icon.ico` 嵌入 `MappyZ`。
- [ ] 将当前未跟踪的 `icon.ico` 正式纳入版本库，资源和 `.rc` 都只能引用仓库内相对路径。
- [ ] 托盘图标与 `QApplication::windowIcon` 使用同一资源，不依赖工作目录或本机绝对路径。
- [ ] Windows 构建增加最小 `.rc` 资源文件，将 `icon.ico` 设置为可执行文件图标。
- [ ] `.rc` 仅在 `WIN32` 下加入 target source，其他平台构建不引用 Windows resource compiler。
- [ ] CMake 安装/部署不需要额外复制松散的 `icon.ico` 才能显示托盘图标。

## Priority 5: CMake And Testability

- [ ] `find_package(Qt6 REQUIRED COMPONENTS Quick Widgets)`。
- [ ] `MappyZ` 链接 `Qt6::Widgets`。
- [ ] 将两个 controller 加入适合测试复用的 desktop target；如果不新增 library，则主程序和测试目标显式共享对应源文件。
- [ ] 新增独立 Desktop 测试入口；不要修改现有 `QmlSmokeTests` 和 `UIBridgeTests` 的 application 类型。
- [ ] `ZWindowLifecycleController` 测试继续使用 `QGuiApplication` + offscreen 和真实 `QWindow`，不构造 Widgets 托盘对象。
- [ ] `ZSystemTrayController` 的平台可用性和原生菜单展示不作为 CI 自动化前提，依靠装配测试和 Windows 手动验收。
- [ ] 不在 offscreen 测试中断言 `QSystemTrayIcon::isSystemTrayAvailable()` 为 true。

Tests：

- [ ] Close 事件被拦截后窗口隐藏，事件未导致应用退出。
- [ ] Minimize 后窗口隐藏。
- [ ] `RestoreWindow()` 清除 minimized 状态并重新显示窗口。
- [ ] 重复 Close/Minimize/Restore 不崩溃且状态一致。
- [ ] 真实退出状态下不再把 Close 转换为 Hide。
- [ ] `bTrayAvailable == false` 时 Close 不被 event filter 拦截。
- [ ] `bTrayAvailable == false` 时关闭最后一个窗口会退出应用，不留下不可访问的后台进程。
- [ ] 托盘 adapter 的 `Exit` action 只发射一次 `ExitRequested()`；若 offscreen 平台无法稳定构造系统托盘，此项放入 Windows integration/manual 验收。
- [ ] QML smoke 在 `visible: false` 下仍能完成加载和 Runtime 初始化，不产生新增 warning。

## Priority 6: Manual Acceptance

- [ ] 启动应用时不闪现主窗口，托盘中出现 `icon.ico` 图标。
- [ ] 启动隐藏后手柄输入与已有映射继续工作。
- [ ] 从托盘点击图标可以恢复主窗口。
- [ ] 点击最小化按钮后主窗口从任务栏隐藏，托盘图标仍存在，映射继续工作。
- [ ] 观察 queued hide 是否产生明显的重复闪烁或任务栏残留；允许系统原生的一次最小化动画，不接受窗口再次弹回或持续闪烁。
- [ ] 点击关闭按钮后行为与最小化到托盘一致，映射继续工作。
- [ ] 托盘右键菜单只有一个 `Exit` 项。
- [ ] 点击 `Exit` 后进程退出，托盘不残留无效图标。
- [ ] 系统不提供托盘时，应用启动后主窗口可见且可正常关闭/退出。
- [ ] Windows Explorer、任务栏、窗口标题栏和托盘均显示 `icon.ico`。

## Definition Of Done

- [ ] 桌面托盘生命周期由单一 C++ 控制器负责，没有 QML/C++ 双重关闭逻辑。
- [ ] Close 和 Minimize 不影响 Runtime；只有明确的 `Exit` 才停止并退出。
- [ ] 隐藏窗口后始终存在恢复入口，不会产生不可访问的后台进程。
- [ ] 图标通过资源系统加载，不包含本机路径。
- [ ] Debug 构建通过。
- [ ] 相关自动化测试和 QML smoke 全部通过。
- [ ] 手动验收全部通过。
