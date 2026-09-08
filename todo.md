# Settings UI And Startup Preference

## Goal

- [ ] 在配置选择器的 Delete 右侧增加 Settings 入口。
- [ ] 点击 Settings 后打开应用级设置界面。
- [ ] 设置界面采用桌面 Linux 设置应用常见的双栏结构：左侧分类导航，右侧设置内容。
- [ ] 本轮只实现 General 分类和 Start minimized 选项。
- [ ] Start minimized 默认启用，并持久化到独立的应用设置文件。
- [ ] 设置只控制下一次启动时的初始窗口可见性，不改变当前窗口，也不改变关闭或最小化到托盘的既有行为。

## Scope

### In Scope

- [ ] 新增应用设置管理器及持久化。
- [ ] 新增设置对话框、左侧分类导航和基本设置页。
- [ ] 将设置入口接入 ProfileSelector、TopBar 和 Main.qml。
- [ ] 将启动时是否隐藏主窗口改为由设置管理器决定。
- [ ] 增加设置管理器、启动策略和 QML 交互测试。

### Out Of Scope

- [ ] 不增加新的映射配置字段。
- [ ] 不把应用设置写入 mapping profile JSON。
- [ ] 不实现主题、语言、自动启动、更新、日志等其他设置页。
- [ ] 不修改关闭到托盘、最小化到托盘、托盘恢复和退出语义。
- [ ] 不引入第二个原生设置窗口，不重构现有主界面。
- [ ] 不修改现有 profile 存储位置或应用身份信息。

## Architecture

### Settings Ownership

- [ ] 新增 source/App/SettingsManager.h 和 source/App/SettingsManager.cpp。
- [ ] 定义 ZSettingsManager final : public QObject，作为应用级设置的唯一权威来源。
- [ ] 设置管理器归属 Desktop/App 层，不放入 Core、Runtime、ProfileManager 或 MappingProfile。
- [ ] Main.cpp 创建唯一的 ZSettingsManager 实例，并将同一实例同时用于启动决策和 QML 展示。
- [ ] 通过 context property settingsManager 注入 QML，避免再增加只转发一个属性的控制器。
- [ ] 保证 SettingsManager 生命周期长于 QQmlApplicationEngine 和根窗口。

### Desktop Targets

- [ ] 不创建把全部桌面代码统一链接到 Qt Widgets 的粗粒度 MappyZDesktop target。
- [ ] 新增 MappyZDesktopCore 静态库，承载 SettingsManager、StartupPolicy 和 WindowLifecycleController，仅链接 Qt6::Core 与 Qt6::Gui。
- [ ] 新增 MappyZDesktopTray 静态库，仅承载 SystemTrayController，并链接 Qt6::Core、Qt6::Gui 与 Qt6::Widgets。
- [ ] 主程序和 DesktopTests 链接上述两个 target，不再分别重复编译这些源文件。
- [ ] 保持设置管理、启动策略和窗口生命周期可在不依赖 Qt Widgets 的目标中独立编译和测试。
- [ ] 保持 MappyZApp、Core、Runtime 和 UI Bridge 不依赖 Qt Widgets。

### Public API

- [ ] ZSettingsManager 提供以下语义 API：

    Q_PROPERTY(bool startMinimized READ StartMinimized NOTIFY StartMinimizedChanged)

    bool StartMinimized() const;

    Q_INVOKABLE bool SetStartMinimized(bool bEnabled);

    signals:
        void StartMinimizedChanged();
        void SettingsError(const QString& Message);

- [ ] 生产构造使用默认设置路径。
- [ ] 测试构造允许注入独立 INI 文件路径，不修改全局 QStandardPaths 测试环境。
- [ ] QML 只读 startMinimized，并通过 SetStartMinimized 修改，避免 QML 成为第二份状态源。

## Persistence

- [ ] 使用 QSettings::IniFormat，不使用 Windows Registry。
- [ ] 默认文件位于 QStandardPaths::AppConfigLocation 下的 settings.ini。
- [ ] 该路径与 profile 使用的 QStandardPaths::AppDataLocation 有意分离：应用偏好属于配置，映射 profile 属于用户数据。
- [ ] Windows 上两者可能落在相同父目录；Linux 上分别位于配置目录和数据目录，这是预期的跨平台语义，不为目录表面统一而改用 AppDataLocation。
- [ ] 使用稳定键名 general/startMinimized。
- [ ] 键不存在时返回 true，确保首次启动默认最小化。
- [ ] 首次仅读取默认值时不强制创建文件；用户实际修改后再写入。
- [ ] 写入后调用 sync，并检查 QSettings::status。
- [ ] 写入成功后才更新内存缓存并发射一次 StartMinimizedChanged。
- [ ] 写入失败时保留原缓存值，返回 false，并发射 SettingsError。
- [ ] 重复设置相同值返回 true，不写盘，不发射 changed signal。
- [ ] 读取到缺失或非法值时安全回退为 true，并通过 qWarning 输出一次可诊断警告。
- [ ] 设置读写失败也使用 qWarning 进入 Qt 日志体系；不使用仅在控制台可见的 fprintf(stderr) 作为唯一诊断通道。
- [ ] 修改已知键时保留文件中的未知键，为后续设置扩展和版本兼容留出空间。
- [ ] 本轮不得调用 setOrganizationName 或改变 applicationName，避免间接迁移现有 profile 与设置目录。

## Startup Policy

- [ ] 新增 source/App/StartupPolicy.h，在 MappyZ 命名空间中提供无 QObject 依赖的自由函数 ShouldShowMainWindow(bTrayAvailable, bStartMinimized)。
- [ ] StartupPolicy 保持纯函数，不作为 ZWindowLifecycleController 的 static 方法；窗口事件处理和进程启动决策是两个独立职责。
- [ ] StartupPolicy 纳入 MappyZDesktopCore，并可由测试直接调用。
- [ ] 全新安装第一次启动也遵守默认值 true：托盘可用时主窗口保持隐藏。这是本轮明确的产品行为，不增加 first-run 例外状态。
- [ ] 首次启动的可发现入口是系统托盘图标；首次运行引导或系统通知不在本轮范围内。
- [ ] 启动行为使用以下真值表：

| Tray available | Start minimized | Main window |
| --- | --- | --- |
| false | false | show |
| false | true | show |
| true | false | show |
| true | true | hide |

- [ ] Main.cpp 必须在决定根窗口 show 或 hide 之前构造并读取 SettingsManager。
- [ ] Main.qml 的 visible 继续固定为 false，防止 QML 创建阶段先显示再隐藏造成闪烁。
- [ ] Main.qml 不得将 visible 绑定到 settingsManager.startMinimized。
- [ ] 根 QWindow 创建并交给 WindowLifecycleController 后，仅由 Main.cpp 根据启动策略执行一次 show 或保持 hidden。
- [ ] 运行期间修改 startMinimized 不触发 MainWindow.show、hide 或 visibility binding 重算。
- [ ] 托盘可用时仍始终创建并显示托盘图标。
- [ ] 托盘不可用时无条件显示主窗口，不能因 Start minimized 导致应用无可见入口。
- [ ] 修改 Start minimized 只影响下一次进程启动；当前窗口不立即隐藏或显示。
- [ ] 关闭按钮和最小化按钮继续隐藏到托盘，与该选项无关。

## QML Structure

### Files

- [ ] 新增 ui/settings/SettingsDialog.qml。
- [ ] 新增 ui/settings/GeneralSettingsPage.qml。
- [ ] 将新文件注册到现有 QML module，并保持目录 alias 与 import 方式一致。

### Entry Point

- [ ] 在 ProfileSelector.qml 的 Delete 右侧增加 Settings 按钮。
- [ ] Settings 是应用级入口，不依赖当前 profile 数量、当前 profile 或 rename 状态。
- [ ] 即使只有一个 profile、Delete 被禁用，Settings 仍保持可用。
- [ ] ProfileSelector 发射 settingsRequested 语义信号。
- [ ] TopBar 只负责转发 settingsRequested，不直接创建设置页或持有设置状态。
- [ ] Main.qml 接收信号并打开 SettingsDialog。
- [ ] 打开设置前关闭 profile 下拉层并取消未提交的 rename 状态，防止浮层重叠。
- [ ] 如果映射选择器已打开，先关闭映射选择器，再打开设置界面。

### Dialog Layout

- [ ] 使用现有应用内 modal overlay 风格，不创建第二个原生 Window。
- [ ] 对话框包含明确标题 Settings 和唯一关闭入口。
- [ ] Escape 与 Close 按钮均关闭对话框，不修改设置。
- [ ] 不增加 Apply、Save 或 Cancel；设置项修改后立即持久化。
- [ ] 主内容采用两栏布局：
- [ ] 左栏固定或受约束宽度约 220 px，展示大尺寸分类条目。
- [ ] 右栏占据剩余宽度，展示当前分类内容。
- [ ] 两栏之间有清晰分隔，不使用嵌套卡片堆叠。
- [ ] 左侧本轮只有 General，行高约 52 至 60 px，具备选中高亮、hover 和键盘焦点状态。
- [ ] 对话框重新打开时默认选中 General。
- [ ] 在最小窗口尺寸下允许内容区收缩，但标题、设置行和关闭按钮不得裁切或重叠。

### General Page

- [ ] 页面标题为 General。
- [ ] 增加 Start minimized 设置行。
- [ ] 设置行说明为 Start MappyZ in the system tray. Takes effect on next launch.
- [ ] 使用标准 CheckBox，默认显示 checked。
- [ ] checked 始终反映 settingsManager.startMinimized，不在页面中维护重复缓存。
- [ ] 用户点击时只调用一次 settingsManager.SetStartMinimized(desiredValue)。
- [ ] 优先使用 CheckBox 的 nextCheckState 或等价的权威状态回写方案，避免 checked binding 与点击切换形成反馈循环。
- [ ] 保存失败时 CheckBox 恢复管理器中的旧值，并显示页面内错误反馈。
- [ ] 保存成功不弹出阻塞式提示；勾选状态本身即为反馈。

## Behavior Details

- [ ] 设置对话框重复打开和关闭不重新创建 SettingsManager。
- [ ] 多次打开后 CheckBox 始终显示最近一次成功持久化的值。
- [ ] profile create、rename、delete、switch 和 autosave 不得改写应用设置。
- [ ] Start minimized 的错误不得污染 profileMessage、profileSaveState 或 mapping profile 日志。
- [ ] SettingsError 仅用于设置相关失败，GeneralSettingsPage 在可见时显示对应 inline error。
- [ ] 设置对话框关闭后不遗留键盘焦点、遮罩或不可点击区域。

## Tests

### SettingsManager Tests

- [ ] 空设置文件或文件不存在时 StartMinimized() == true。
- [ ] 设置 false 后重新构造管理器仍读取 false。
- [ ] 设置 true 后重新构造管理器仍读取 true。
- [ ] 重复设置相同值不写入额外状态且不发射 StartMinimizedChanged。
- [ ] 成功修改只发射一次 StartMinimizedChanged，且 signal slot 中读取到的是新值。
- [ ] 不可写路径下设置失败返回 false、缓存保持旧值并发射 SettingsError。
- [ ] 修改 general/startMinimized 后未知键仍被保留。
- [ ] 两个注入不同路径的管理器互不影响。
- [ ] 非法持久化值安全回退为 true。

### Startup Policy Tests

- [ ] 覆盖 ShouldShowMainWindow 的四种真值组合。
- [ ] 明确验证 tray unavailable 时即使 Start minimized 为 true 也显示窗口。
- [ ] 明确验证 tray available 且 Start minimized 为 true 时隐藏窗口。

### QML And Integration Tests

- [ ] QML smoke 测试注入使用临时路径的真实 SettingsManager 或接口一致的测试对象，绝不读取或改写生产设置。
- [ ] ProfileSelector 的 Settings 按钮位于 Delete 右侧。
- [ ] 单 profile 且 Delete 禁用时 Settings 仍可点击。
- [ ] 点击 Settings 打开对话框，并默认选中 General。
- [ ] 首次打开时 Start minimized 显示为 checked。
- [ ] 点击 CheckBox 只调用一次 setter。
- [ ] setter 失败时 UI 恢复旧状态并显示错误。
- [ ] 关闭并重新打开后 UI 与 SettingsManager 状态一致。
- [ ] Escape 和 Close 均能关闭设置页且没有 QML warning。
- [ ] 最小窗口尺寸下两栏、设置行和底部关闭按钮不裁切。

## CMake And Resources

- [ ] 将 SettingsManager、StartupPolicy 和 WindowLifecycleController 接入 MappyZDesktopCore。
- [ ] 将 SystemTrayController 接入 MappyZDesktopTray。
- [ ] 将 SettingsDialog.qml 和 GeneralSettingsPage.qml 注册到 qt_add_qml_module。
- [ ] 主程序链接 MappyZDesktopCore 与 MappyZDesktopTray。
- [ ] DesktopTests 链接 MappyZDesktopCore、MappyZDesktopTray 和所需 Qt Test 组件。
- [ ] 为 SettingsManager 与 StartupPolicy 提供不需要 Qt Widgets 的测试挂载点；真实托盘适配器测试仍可使用 QApplication。
- [ ] 不增加新的第三方依赖。

## Manual Acceptance

- [ ] 全新配置启动时，应用默认隐藏到托盘。
- [ ] 从托盘恢复窗口，打开 Settings，Start minimized 默认勾选。
- [ ] 取消勾选、退出并重启后，主窗口直接显示且托盘仍存在。
- [ ] 再次勾选、退出并重启后，主窗口隐藏到托盘。
- [ ] 系统托盘不可用时，应用始终显示主窗口。
- [ ] 修改设置不会立即隐藏当前窗口。
- [ ] 关闭和最小化仍按既有规则隐藏到托盘。
- [ ] Settings 位于 Delete 右侧，且不受 profile CRUD 状态影响。
- [ ] 设置页在最小窗口和默认窗口尺寸下无重叠、裁切和布局跳变。

## Completion

- [ ] SettingsManager 单元测试通过。
- [ ] Desktop lifecycle 与 tray 现有测试无回归。
- [ ] UI bridge 和 QML smoke 测试通过。
- [ ] 全量构建与测试通过。
- [ ] 仅在全部自动化测试和 Windows 托盘手动验收通过后提交。
