# Automatic Profile Switching

## Goal

- [ ] 将稳定 ID 为 `default` 的配置定义为系统回退配置，并在 UI 与 `ZProfileManager` 两层禁止删除。
- [ ] 在 Settings 左侧菜单增加 `Automatic Profile Switching` 分类。
- [ ] 提供自动切换规则表格、添加、删除和启用/禁用能力。
- [ ] 添加规则时可手动输入进程名，也可从当前运行的可见应用列表中选择。
- [ ] Windows 前台应用变化时，按启用规则自动切换到对应配置；没有有效匹配时切换到 Default。
- [ ] 自动切换复用现有 profile 激活、脏配置保存、Runtime snapshot 更新和托盘同步链路，不创建第二套配置状态。

## Product Decisions

- [ ] Default 的身份只由稳定 ID `default` 决定，不依赖显示名或文件名。
- [ ] 本轮禁止删除和重命名 Default：Default 是系统回退配置，显示名固定不可修改。自动回退始终按 ID `default` 查找。仍允许编辑其映射并保存。
- [ ] 进程匹配使用可执行文件 basename 的大小写无关精确匹配，例如 `eldenring.exe`。
- [ ] 不支持通配符、正则、窗口标题匹配、路径前缀或子串匹配，避免误命中。
- [ ] 同一规范化进程名最多存在一条规则；重复添加时拒绝并显示明确错误，不引入隐式优先级。
- [ ] 规则表中的勾选框只控制该规则是否参与匹配；未勾选等同于无规则命中。
- [ ] 未命中、命中规则被禁用、规则引用的配置不存在或前台进程无法识别时，目标均为 Default。
- [ ] 目标配置已经是当前配置时不重复加载、不重复保存、不发出 profile changed 信号。
- [ ] 自动切换是持续运行能力，不增加总开关；逐条 `Enabled` 即为唯一开关来源。
- [ ] 自动切换失败时保留当前配置，不强行覆盖 Runtime，并记录一次可诊断错误；同一前台进程不进行高频重试。
- [ ] 本轮 Windows 完整实现；非 Windows 平台提供明确的 unavailable 状态，不伪造进程数据。

## Scope

### In Scope

- [ ] Default 配置创建、恢复和不可删除约束。
- [ ] 自动切换规则的数据结构、JSON 持久化、列表模型和 AppController 编排。
- [ ] Windows 前台应用事件监听。
- [ ] Windows 可见顶层应用枚举及进程选择界面。
- [ ] Settings 新分类、规则 TableView、添加规则对话框和进程选择对话框。
- [ ] 自动切换、profile CRUD 联动、错误反馈和测试。

### Out Of Scope

- [ ] 不支持窗口标题、窗口类名、命令行参数、完整路径或 UWP 包身份匹配。
- [ ] 不支持一条规则匹配多个进程，也不支持多个条件的 AND/OR 组合。
- [ ] 不支持规则拖动排序或优先级；进程名唯一性使优先级没有必要。
- [ ] 不支持按设备、游戏手柄型号、桌面、用户或时间切换配置。
- [ ] 不监听子进程树，不推断 launcher 与游戏进程的父子关系。
- [ ] 不扫描系统服务、后台进程或无可见顶层窗口的进程。
- [ ] 不读取进程图标，不增加进程搜索、收藏或历史记录。
- [ ] 不实现 Linux/macOS 的前台窗口平台适配器，但接口需允许后续增加。
- [ ] 不修改 mapping profile JSON schema；自动规则是应用级数据，不写入单个 profile。

## Architecture

### Responsibility Boundaries

- [ ] `ZProfileManager` 继续作为 profile 集合、active profile 和磁盘文件的唯一管理者，并负责 Default 不变量。
- [ ] 新增纯数据 `SAutoProfileRule`，只描述规则，不依赖 QObject、QML 或平台 API。
- [ ] 新增 `ZAutoProfileRuleStore`，只负责规则 JSON 的读取、校验和原子写入。
- [ ] 新增 `ZAutoProfileRuleModel : QAbstractListModel`，只负责向 QML 暴露权威规则快照，不直接切换 profile。
- [ ] `ZAppController` 负责规则 mutation、持久化、profile 引用校验以及调用现有 `switchProfile()` 链路。
- [ ] 新增 `ZForegroundApplicationService : QObject`，负责把平台前台窗口事件转成 Qt 语义信号，并按需提供运行应用候选列表。
- [ ] Windows API 封装在 `ZWindowsForegroundApplicationSource` 中；UI Bridge、Runtime 和 Core 不包含 Windows 头。
- [ ] `Main.cpp` 只做对象装配：创建 foreground service、注入 QML、连接其语义信号到 `ZAppController`，不实现匹配规则。

### Data Types

- [ ] 在 `source/App/AutoProfileRule.h` 定义：

```cpp
struct SAutoProfileRule
{
    StdString Id;          // 独立稳定 ID，使用 UUID 字符串
    bool bEnabled = true;
    StdString ProcessName; // 规范化后的可执行文件 basename
    StdString ProfileId;   // 引用 SProfileInfo::Id
};

struct SForegroundApplication
{
    StdString ProcessName;
    StdString DisplayName;
    uint32 ProcessId = 0;
};
```

- [ ] 规则 ID 不使用进程名，避免未来修改匹配条件时破坏行身份和 QML selection。
- [ ] `ProfileId` 持久化稳定 ID，不持久化 profile 显示名或文件路径。
- [ ] 增加单一规范化函数 `NormalizeProcessName()`，供手工输入、平台枚举、持久化加载和前台事件共同使用。
- [ ] Windows 规范化规则：trim、移除成对引号、若输入包含路径则只取 basename、ASCII 大小写转小写、保留 `.exe`。
- [ ] 空字符串、`.`、`..`、带通配符或规范化后不含有效文件名的输入视为非法。

### Target Placement

- [ ] `AutoProfileRule.h` 与 `AutoProfileRuleStore.cpp/.h` 放入 `MappyZApp`；存储使用 `std::filesystem`、标准流和项目已有 JSON 库，不为该模块给 `MappyZApp` 新增 Qt 依赖。
- [ ] `ForegroundApplicationService.cpp/.h` 放入 `MappyZDesktopCore`，公共接口仅依赖 Qt Core。
- [ ] `WindowsForegroundApplicationSource.cpp/.h` 作为 `MappyZDesktopCore` 的 `WIN32` 条件源编译，并在该 target 上链接所需 Win32 系统库；本轮不为一个实现额外拆出空壳 target。
- [ ] `AutoProfileRuleModel.cpp/.h` 放入 `MappyZUIBridge`。
- [ ] 不让 `MappyZApp`、Runtime 或 UI Bridge 反向依赖 `Qt6::Widgets`。

## Default Profile Invariant

### Initialization

- [ ] 在 `ZProfileManager` 中定义稳定常量 `DefaultProfileId = "default"`，所有 fallback 和保护逻辑复用该常量。
- [ ] `Initialize()` 不再只在 profile 列表为空时创建 Default；发现配置后必须验证 ID `default` 存在。
- [ ] 若 ID `default` 不存在，创建空的 Default profile 并加入列表，不覆盖任何现有配置。
- [ ] 优先使用 `default.json`；若该路径已被无法采用的文件占用，选择未占用文件名保存，但 profile 内部 ID 仍固定为 `default`。
- [ ] 若无法建立 Default，本次 profile 初始化失败，不能进入没有安全 fallback 的半可用状态。
- [ ] active profile 记录缺失或失效时始终回退 ID `default`，不再回退排序后的任意第一项。

### Delete Protection

- [ ] 新增 `IsDefaultProfile(ProfileId)` 或等价语义 API，禁止各层复制字符串判断。
- [ ] `CanDeleteActiveProfile()` 同时要求 active ID 不是 `default`；不能只判断配置数量。
- [ ] `DeleteActiveProfile()` 在 Runtime 层再次拒绝删除 ID `default`，即使调用方绕过 UI 也不能删除文件。
- [ ] `ZAppController::CanDeleteProfile()` 反映 manager 的权威结果，Default 选中时 Delete 按钮禁用。
- [ ] 删除非 Default 配置后，fallback 固定切换到 Default，行为不再依赖名称排序。
- [ ] 删除非 Default 配置时同步移除所有引用该 ProfileId 的自动切换规则，并与 profile 删除结果一起刷新 UI。
- [ ] 若删除 profile 已成功但规则文件清理保存失败，不回滚已删除文件；报告规则清理错误并确保残留规则在运行时被视为失效、回退 Default。

## Rule Persistence

### Storage

- [ ] 规则保存在 `QStandardPaths::AppConfigLocation/automatic_profile_rules.json`，与 `settings.ini` 同属应用行为配置，但不混入 QSettings 标量键。
- [ ] 测试构造允许注入独立文件路径，不修改全局标准目录。
- [ ] JSON 顶层包含 `schema_version` 和 `rules` 数组，为后续字段迁移保留版本入口；当前 schema version 为 `1`。
- [ ] 写入使用同目录临时文件 + flush/close + rename replace 的原子替换策略，避免进程中断留下半个 JSON。
- [ ] 保存成功后才提交 model 内存状态并发 changed signal；失败时保持旧规则快照。

### Validation And Recovery

- [ ] 加载时逐条验证 ID、Enabled、ProcessName 和 ProfileId 类型。
- [ ] 规则 ID 缺失或重复时为该条生成新 UUID，并在下一次成功 mutation 时写回修复结果。
- [ ] ProcessName 按统一函数重新规范化；规范化失败的规则跳过并输出一次 `qWarning`。
- [ ] 同一规范化进程名出现重复规则时保留文件中第一条，跳过后续项并警告。
- [ ] 引用不存在 profile 的规则保留在文件与表格中，但标记为 invalid、运行时不参与匹配，便于用户看见并删除问题规则。
- [ ] JSON 文件不存在视为空规则集，不创建文件、不报错。
- [ ] JSON 整体损坏时不覆盖原文件，加载为空规则集并向 UI 暴露一次错误；用户首次成功修改时才以有效内容替换。
- [ ] 未知顶层字段和规则字段在本轮可以忽略；不得因未知字段导致崩溃。

## Rule Model And Controller API

### Model Roles

- [ ] `ZAutoProfileRuleModel` 提供以下 roles：

```text
ruleId
enabled
processName
profileId
profileName
valid
statusText
```

- [ ] `profileName` 由当前 profile 列表解析，仅用于显示；匹配和保存仍使用 `profileId`。
- [ ] profile 不存在时 `valid == false`，`profileName` 显示 `Missing profile`，`statusText` 给出可诊断原因。
- [ ] model 提供 `RuleIdAt(row)` 等安全访问 API，不让 QML 使用魔法 role 编号。

### AppController Properties And Commands

- [ ] `ZAppController` 增加：

```cpp
Q_PROPERTY(ZAutoProfileRuleModel* autoProfileRuleModel
           READ AutoProfileRuleModel CONSTANT)
Q_PROPERTY(QString foregroundProcessName
           READ ForegroundProcessName NOTIFY foregroundProcessChanged)
Q_PROPERTY(QString automaticProfileMessage
           READ AutomaticProfileMessage NOTIFY automaticProfileStatusChanged)

Q_INVOKABLE bool addAutomaticProfileRule(QString processName, QString profileId);
Q_INVOKABLE bool removeAutomaticProfileRule(QString ruleId);
Q_INVOKABLE bool setAutomaticProfileRuleEnabled(QString ruleId, bool enabled);

public slots:
    void handleForegroundApplicationChanged(QString processName);
```

- [ ] 自动切换错误和状态使用独立属性/信号，不污染 `profileMessage`、`profileSaveState` 或设置保存错误。
- [ ] 增加独立规则路径解析 helper：生产路径取 `AppConfigLocation/automatic_profile_rules.json`；测试构造允许显式注入规则文件路径，不能借用 profile 目录或读写真实用户配置。
- [ ] `initializeProfiles()` 成功后加载规则、向 model 提交快照，并允许开始求值。
- [ ] 添加规则前验证进程名、profile ID 存在且不是空值，并拒绝规范化进程名重复。
- [ ] 允许规则目标为 Default，虽然它等价于显式 fallback；不增加不必要的特殊限制。
- [ ] checkbox mutation 成功持久化后才更新 model；失败时 QML 自动保持旧值。
- [ ] 删除规则使用稳定 rule ID，不使用当前 row，避免排序/刷新造成误删。
- [ ] profile rename 不修改规则，只刷新 `profileName` role；profile create/switch 不改规则。

## Foreground Application Service

### Public Contract

- [ ] 定义平台无关 source 接口与 Qt service；source 提供 Start/Stop、当前前台应用和可见应用枚举，service 提供 Qt signal、候选列表和 `refreshApplicationEntries()`。
- [ ] service 向 QML 暴露 `applicationEntries`、`statusMessage` 和 `available`，每个候选项包含 `processName`、`displayName`、`processId`。
- [ ] service 将平台回调安全投递到自身 Qt 线程，QML model 与 `ZAppController` 不在 Win32 回调栈内直接修改。
- [ ] 对连续相同的规范化进程名去重，只发射一次 `foregroundApplicationChanged`。
- [ ] `Start()` 后主动读取并发布当前前台应用，不能等下一次窗口切换才生效。
- [ ] `Stop()` 幂等，析构时必须解除系统 hook，禁止回调访问已销毁 QObject。

### Windows Foreground Tracking

- [ ] 使用 `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, ..., WINEVENT_OUTOFCONTEXT)` 接收前台窗口变化，不使用高频轮询。
- [ ] 回调中只提取 HWND/PID 并安排 Qt queued 处理，不执行 profile I/O 或 QML 更新。
- [ ] 通过 `GetWindowThreadProcessId`、`OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` 和 `QueryFullProcessImageNameW` 获取可执行文件名。
- [ ] 系统窗口、进程已退出、权限拒绝或路径解析失败时发布空进程名，随后由匹配策略回退 Default。
- [ ] hook 安装失败时 service 返回 unavailable 并记录 `qWarning`；本轮不静默退化为高频 timer。
- [ ] Windows source 实现不得持有 `ZAppController` 或 profile manager 指针。

### Running Application Enumeration

- [ ] 点击进程选择按钮时才调用 `refreshApplicationEntries()`；不持续扫描进程列表。
- [ ] 使用 `EnumWindows` 枚举当前可见、非 tool window、具有非空标题的顶层窗口。
- [ ] 跳过 MappyZ 自身窗口和无法查询进程名称的窗口。
- [ ] 按规范化 ProcessName 去重；同一进程多个窗口只显示一项。
- [ ] 候选项按显示名称、再按进程名进行大小写无关排序，保证列表稳定。
- [ ] 选择后只把 processName 回填输入框；不把完整本机路径暴露到 QML 或日志。
- [ ] 枚举为空显示空状态并允许返回手工输入；枚举失败不关闭添加对话框，也不清空用户输入。

## Automatic Switching Behavior

### Matching

- [ ] `handleForegroundApplicationChanged()` 先规范化进程名，再从规则权威快照中进行一次精确查找。
- [ ] 只有 `enabled && valid && normalizedName == rule.ProcessName` 的规则能够命中。
- [ ] 命中后目标为规则的 ProfileId；否则目标固定为 `default`。
- [ ] 求值逻辑抽为无 UI、无平台依赖的纯函数 `ResolveAutomaticProfileId(processName, rules, availableProfileIds)`。
- [ ] `ZAppController` 只在 profile 已初始化后执行切换；初始化前收到的最后一个前台进程名需要缓存，并在初始化成功后求值一次。

### Applying A Match

- [ ] 自动切换必须调用与手动切换相同的内部切换 helper，复用保存当前脏配置、`ActivateProfile()`、Runtime `ReplaceProfile()`、MappingRuleModel 刷新及语义信号。
- [ ] 将现有 `switchProfile(QString)` 核心逻辑抽为私有 `SwitchProfileInternal(ProfileId, Reason)`；手动、托盘和自动切换只传入不同 reason。
- [ ] 自动切换不停止/重启 input backend，不清空设备列表，不重建 RuntimeHost。
- [ ] 成功自动切换只写一条 lifecycle 日志，不为相同前台事件重复写日志。
- [ ] 回到 Default 仅在当前不是 Default 时执行并记录一次。
- [ ] 切换失败保留当前 profile、profile model 和 Runtime snapshot；错误消息包含目标 profile 和失败原因。
- [ ] 同一前台进程发生过切换失败后，直到前台进程、规则或 profile 列表变化前不重复尝试。
- [ ] 添加、删除、启用/禁用规则成功后立即对当前缓存的前台进程重新求值。
- [ ] 删除/创建/重命名 profile 后刷新规则有效性；删除目标配置后立即求值并回到 Default。

## Settings UI

### Navigation And Page

- [ ] 在 `SettingsDialog.qml` 左侧 General 下增加 `Automatic Profile Switching` 大条目。
- [ ] `SettingsDialog` 新增并显式接收 `appController` 与 `foregroundApplicationService` required properties；`Main.qml` 传入现有 context object，页面内部不直接依赖未声明的全局名称。
- [ ] 分类选择使用稳定 string key，不依赖易变化的 row 数字。
- [ ] 设置窗口重新打开时默认仍进入 General；点击新条目后右侧切换到自动配置页。
- [ ] 新增 `ui/settings/AutomaticProfileSettingsPage.qml`，页面标题、工具栏、表格和错误反馈均位于右侧内容区。

### Rules Table

- [ ] 使用 Qt Quick `TableView`，固定列为 Enabled、Process、Profile、Status。
- [ ] Enabled 使用可操作 CheckBox；Process 与 Profile 本轮只读；Status 仅在引用失效或保存错误时显示。
- [ ] 表头固定，内容超出时纵向滚动；窄窗口优先压缩 Status，Process/Profile 使用 elide。
- [ ] 单击行选中，selection 使用 ruleId 作为权威身份，不长期缓存 row index。
- [ ] 顶部工具栏包含 Add 和 Delete；Delete 无选中行时禁用。
- [ ] 删除成功后选择相邻行；表格为空时显示 `No automatic switching rules yet.`。
- [ ] 缺失 profile 的规则显示 `Missing profile`，Enabled 可关闭，Delete 仍可用。
- [ ] checkbox 调用失败时恢复 model 权威值并显示 inline error，不在 QML 维护 enabled 副本。

### Add Rule Dialog

- [ ] 新增 `ui/settings/AddAutomaticProfileRuleDialog.qml`，作为 Settings 内二级 modal。
- [ ] 包含 Process name 输入框、输入框右侧 Browse 按钮、Profile 下拉框、Cancel 和 Confirm。
- [ ] Profile 下拉消费 `appController.profileEntries`，显示 name、提交稳定 id。
- [ ] 打开时输入为空，Profile 默认当前 active；active 无效时选择 Default。
- [ ] Confirm 仅在进程名非空且 profile 有效时启用。
- [ ] Confirm 成功后关闭并由 model 自动新增一行；失败保留输入和选择并显示具体错误。
- [ ] 这里是新增一行，不是向 TableView 增加一列；列结构固定。

### Process Picker Dialog

- [ ] 新增 `ui/settings/ProcessPickerDialog.qml`，z 高于 Add dialog。
- [ ] Browse 按需刷新候选列表；列表显示窗口标题/显示名并辅以 executable basename。
- [ ] 单击选择，双击或 Confirm 只回填 processName，不直接创建规则。
- [ ] Cancel 返回 Add dialog并保留原输入和 profile 选择。
- [ ] 无结果时显示 `No visible applications found. Enter a process name manually.`。
- [ ] Escape 按层级关闭 Process Picker、Add dialog、Settings，不能一次关闭多层。

### Main Wiring

- [ ] `Main.cpp` 创建唯一 `ZForegroundApplicationService`，生命周期长于 QML engine 和 AppController。
- [ ] 通过 context property `foregroundApplicationService` 注入同一实例。
- [ ] 将 foreground signal 以 queued connection 连接到 AppController handler。
- [ ] QML 不自行匹配或调用 `switchProfile()`；匹配与切换留在 C++。
- [ ] QML smoke 注入接口一致的 fake service，不枚举测试机真实窗口。

## Error And Status Semantics

- [ ] 规则 CRUD 输入错误显示在自动切换页或 Add dialog，不复用 mapping picker 错误。
- [ ] 前台监听启动失败显示一次状态并记录 `qWarning`；手动 profile 功能仍可用。
- [ ] 进程枚举失败只影响 Browse，不停止已工作的监听。
- [ ] 自动切换失败写 LogModel Error，成功切换写 Info；前台进程变化本身不逐条写日志。
- [ ] 日志只记录 executable basename 和 profile display name，不记录完整进程路径。

## Tests

### ProfileManager

- [ ] 空目录初始化创建并激活 ID `default`。
- [ ] 已有其他配置但缺少 `default` 时补建 Default，且不覆盖现有配置。
- [ ] active state 缺失或无效时回退 Default。
- [ ] active 为 Default 时 `CanDeleteActiveProfile()` 恒为 false。
- [ ] 直接删除 Default 返回错误且文件仍存在。
- [ ] 删除非 Default 后固定激活 Default。
- [ ] Default 可编辑映射并保存，但不可删除、不可重命名，ID 与显示名均固定。

### Rule Store And Policy

- [ ] 文件不存在加载为空；add/save/reload 保留全部字段。
- [ ] 写入失败不提交 mutation，原文件不被截断。
- [ ] 损坏 JSON 不被自动覆盖并返回可诊断错误。
- [ ] 规范化覆盖大小写、空白、引号和完整路径。
- [ ] 重复进程名拒绝；重复/缺失 rule ID 安全修复；未知字段不阻断加载。
- [ ] 启用且有效的精确名称命中；大小写差异命中；子串不命中。
- [ ] disabled、缺失 profile、无规则和空进程均返回 Default。

### AppController

- [ ] initializeProfiles 加载规则并刷新 model。
- [ ] add/remove/setEnabled 成功后持久化、model 与 signal 同步。
- [ ] mutation 保存失败时 model 保持旧值。
- [ ] rename 后 profileName 更新而 profileId 不变。
- [ ] 删除被引用的非 Default profile 后清理对应规则并切换 Default。
- [ ] 前台命中时 Runtime snapshot 与 MappingRuleModel 同步更新。
- [ ] 从匹配应用切到未匹配应用时回到 Default。
- [ ] 初始化前事件只缓存最后一个进程，初始化后求值一次。
- [ ] 连续相同事件不重复切换、保存或写日志。
- [ ] 切换失败保持当前 profile 且不高频重试；条件变化后允许重试。
- [ ] 自动切换不重启 backend、不清空设备模型。

### Foreground Service

- [ ] fake source 验证 Start 后立即发布当前应用。
- [ ] 连续相同名称只发一次 signal。
- [ ] 非 Qt 线程 callback 经 queued dispatch 后才更新 Qt 状态。
- [ ] Stop/析构幂等，停止后 callback 不再转发。
- [ ] refresh 成功时去重排序；失败时保留旧候选并更新状态。
- [ ] Windows helper 覆盖进程名提取、不可访问进程和自身过滤。

### QML

- [ ] Settings 新分类可进入，空表有 empty state。
- [ ] Add dialog 默认选择当前 profile，手工输入可新增规则。
- [ ] 重复进程失败时 dialog 保持打开并显示错误。
- [ ] Browse 调 fake refresh，选择后只回填输入框。
- [ ] checkbox 每次只调用一次 setter，失败恢复旧值。
- [ ] Delete 按 ruleId 删除正确规则，不受 row 刷新影响。
- [ ] 缺失 profile 显示 Missing profile 且可删除。
- [ ] Escape 按层关闭三层 overlay。
- [ ] 最小窗口 `1120x720` 下导航、工具栏、表头和按钮不裁切重叠。
- [ ] QML 无 binding loop、undefined property 或 anchors warning。

## CMake And Platform Integration

- [ ] 规则数据/存储加入 `MappyZApp`，规则 model 加入 `MappyZUIBridge`。
- [ ] foreground service 加入 `MappyZDesktopCore`。
- [ ] Windows source 仅在 `WIN32` 下编译并链接 `user32`；额外系统库在 target 层显式声明。
- [ ] 非 Windows 构建提供 unavailable source 或 factory 错误，不能出现 unresolved symbol。
- [ ] 注册三个新增 settings QML 文件。
- [ ] 测试使用注入目录与 fake foreground source，不读写生产配置或枚举真实窗口。
- [ ] 不增加第三方依赖。

## Manual Acceptance

- [ ] 首次启动始终有 Default，选中它时 Delete 禁用。
- [ ] 多配置下仍无法通过 UI 或底层命令删除 Default。
- [ ] 自动切换页可添加、启用/禁用和删除规则。
- [ ] 可手工输入进程名，也可 Browse 可见应用并回填 basename。
- [ ] 聚焦匹配程序后切换配置，映射立即生效且设备列表不中断。
- [ ] 切到未配置程序或禁用规则后回到 Default。
- [ ] 删除规则目标 profile 后规则清理且回到 Default。
- [ ] 托盘勾选、顶部配置名、Inspector 和映射面板随自动切换同步刷新。
- [ ] Browse、设置页与主窗口之间切焦点不产生崩溃或重复切换风暴。
- [ ] 重启后无需打开 Settings，规则即可生效。

## Execution Order

- [ ] P0：Default 稳定身份、补建、回退和底层不可删除。
- [ ] P1：规则数据、进程名规范化、JSON store 和纯匹配策略。
- [ ] P2：RuleModel 与 AppController CRUD/profile 联动。
- [ ] P3：foreground service、fake source 和 Windows 事件/枚举适配器。
- [ ] P4：Main.cpp 装配并跑通自动切换闭环。
- [ ] P5：Settings 分类、TableView、Add dialog 和 Process Picker。
- [ ] P6：自动化测试、Windows 手动验收和错误文案。

## Completion

- [ ] Default 创建、回退和不可删除由底层测试保证。
- [ ] 规则持久化与匹配策略测试通过。
- [ ] Windows 前台监听和应用枚举手动验收通过。
- [ ] AppController、Desktop、UI Bridge 与 QML smoke 无回归。
- [ ] 全量构建和测试通过。
- [ ] 仅在自动切换不会重启输入后端、不会丢设备、不会覆盖脏配置且 Default fallback 可用后提交。
