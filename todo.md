# TODO: Multiple Profiles

## 1. Goal

把当前单配置读写扩展为可持久化的多配置管理：

- [ ] 点击顶部 `Profile` 区域展开全部配置，点击一项立即切换。
- [ ] Profile 右侧提供 `+`、`Rename`、`Delete`。
- [ ] `+` 创建空配置 `Untitled_N` 并立即切换过去。
- [ ] `Rename` 将 Profile 显示区切换为可编辑状态。
- [ ] `Delete` 删除当前配置及其 JSON 文件，然后切换到剩余配置。
- [ ] 托盘右键菜单列出全部配置，当前项带勾，点击后切换配置。
- [ ] 主窗口、托盘、Runtime 和磁盘始终共享同一个当前配置状态。

现有 `ZProfileManager` 从“单文件 JSON 编解码器”扩展为配置目录、配置列表、当前配置和文件生命周期的唯一管理者。`ZAppController` 负责将 manager 返回的 profile snapshot 应用到 `RuntimeHost`，并把状态暴露给 QML 和托盘。

## 2. Final Architecture

```text
AppDataLocation/profiles/*.json
AppDataLocation/profiles/active_profile.txt
                     |
                     v
              ZProfileManager
       list / activate / save / CRUD
                     |
                     v
               ZAppController
       RuntimeHost::ReplaceProfile()
          |                       |
          v                       v
 ProfileSelector.qml     ZSystemTrayController
```

职责固定如下：

- [x] `ZProfileManager`：文件发现、JSON 读写、当前 ID 持久化、创建、激活、重命名、删除。
- [x] `ZAppController`：调用 manager、替换 Runtime snapshot、刷新 QML model、维护 dirty/error UI 状态、发信号。
- [x] `ProfileSelector.qml`：只渲染 controller 状态并调用 invokable，不保存独立配置列表。
- [x] `ZSystemTrayController`：只渲染传入的菜单数据并发出 profile ID，不读取文件、不访问 Runtime。
- [x] `Main.cpp`：连接 controller 与托盘，不复制配置业务规则。

本轮沿用现有 `SMappingProfile`、`ZRuntimeHost::ReplaceProfile()` 和立即自动保存流程；不新增 Profile C++ Model、Repository、Service 或后台任务。

## 3. Storage And Identity Contract

### 3.1 Directory Layout

```text
<QStandardPaths::AppDataLocation>/profiles/
  active_profile.txt
  default.json
  untitled_1.json
  untitled_2.json
```

- [x] 每个配置对应配置目录直属的一份 `.json` 文件；不递归扫描子目录。
- [x] `active_profile.txt` 是 UTF-8 文本，只保存当前 `profile_id`，末尾换行可选。
- [x] 配置显示名使用 JSON 中的 `profile_name`。
- [x] 配置身份使用 JSON 中稳定的 `profile_id`。
- [x] 文件路径保存在 manager 建立的 `SProfileInfo` 中，后续保存和删除只使用该路径。
- [x] 重命名仅修改 `profile_name`；不修改 `profile_id`，也不重命名文件。
- [x] 接受文件名与显示名长期分离，例如 `untitled_1.json` 可以显示为 `My Profile`；稳定 ID 和路径用于持久化及后续自动切换引用。
- [x] 配置名称经首尾空白清理后必须非空，并在列表中忽略大小写唯一。
- [x] 名称唯一性不做 Unicode 正规化，也不折叠名称中间的全角/半角空白；MVP 接受视觉相近但码点不同的非 ASCII 名称。
- [x] 配置列表按名称忽略大小写排序；名称相同时按 ID 排序。QML 与托盘使用该顺序，不再各自排序。

### 3.2 Existing `default.json`

- [x] 现有 `<AppDataLocation>/profiles/default.json` 直接进入配置列表，映射内容保持不变。
- [x] 若文件缺少或使用空 `profile_id`，manager 在内存中使用文件 stem `default` 作为 ID，并在下一次保存时写回 JSON。
- [x] 若文件缺少或使用空 `profile_name`，manager 在内存中使用文件 stem 作为显示名，并在下一次保存时写回 JSON。
- [x] 若当前记录不存在、为空或指向无效配置，优先选择 ID 为 `default` 的配置，否则选择排序后的第一项，并更新 `active_profile.txt`。
- [x] 如果没有任何有效配置，创建空的 `Default`。优先使用 `default.json/default`；路径已被无效文件占用时，使用第一个可用的 `default_N.json/default_N`，不覆盖损坏文件。

### 3.3 Invalid Files

- [x] 枚举前先按完整路径排序，使重复项处理稳定。
- [x] 无法读取、无法解析或 schema 无效的 JSON：写 stderr 日志，保留原文件，不加入列表。
- [x] 重复 `profile_id` 或重复显示名：保留排序后遇到的第一份，记录错误，后续重复项不加入列表且不删除。
- [x] manager 初始化只有在配置目录无法创建、无法枚举，或最终无法得到任何可用配置时返回失败。

### 3.4 Process Ownership

- [x] 生产环境依赖 `Main.cpp` 已有的 `QSharedMemory` 单实例守卫，profiles 目录按单进程独占处理。
- [x] 不为 JSON 或 `active_profile.txt` 增加跨进程文件锁。
- [x] 单元测试为每个 case 注入独立临时目录，不与生产目录或其他 case 并发写同一路径。

## 4. `ZProfileManager` Implementation

目标文件：

- `source/Runtime/ProfileManager.h`
- `source/Runtime/ProfileManager.cpp`
- `tests/Runtime/ProfileManagerTests.cpp`

### 4.1 Public Data Structures

在 `ProfileManager.h` 增加：

```cpp
struct SProfileInfo
{
    StdString Id;
    StdString Name;
    StdPath FilePath;
};

struct SLoadedProfile
{
    SProfileInfo Info;
    SMappingProfile Profile;
};
```

`SLoadedProfile` 用于让 manager 在一次成功操作中同时返回权威描述和可直接应用到 Runtime 的 snapshot，避免调用方操作完成后再次读盘。

### 4.2 Public API

保留现有四个单文件 API：

```cpp
TResult<SMappingProfile> LoadProfile(const StdPath& ProfilePath) const;
TResult<void> SaveProfile(
    const SMappingProfile& Profile,
    const StdPath& ProfilePath) const;
TResult<SMappingProfile> ParseProfileJson(StdStringView JsonText) const;
TResult<StdString> SerializeProfileJson(
    const SMappingProfile& Profile) const;
```

增加集合 API：

```cpp
TResult<SLoadedProfile> Initialize(const StdPath& ProfilesDirectory);

NODISCARD const TVector<SProfileInfo>& GetProfiles() const noexcept;
NODISCARD TOptional<SProfileInfo> GetActiveProfileInfo() const;
NODISCARD bool CanDeleteActiveProfile() const noexcept;

TResult<SLoadedProfile> ActivateProfile(StdStringView ProfileId);
TResult<SLoadedProfile> CreateProfile();
TResult<SLoadedProfile> RenameActiveProfile(
    const SMappingProfile& CurrentSnapshot,
    StdStringView NewName);
TResult<SLoadedProfile> DeleteActiveProfile();
TResult<void> SaveActiveProfile(
    const SMappingProfile& CurrentSnapshot);
```

### 4.3 Manager State

`ZProfileManager` 增加以下私有状态：

```cpp
StdPath ProfilesDirectory;
StdPath ActiveProfileStatePath;
TVector<SProfileInfo> Profiles;
StdString ActiveProfileId;
bool bInitialized = false;
```

- [x] `Initialize()` 成功前，集合操作返回 `InvalidArgument`。
- [x] `GetProfiles()` 返回 manager 已排序的稳定列表。
- [x] `GetActiveProfileInfo()` 按 `ActiveProfileId` 查找并返回副本；未初始化时返回空。
- [x] `CanDeleteActiveProfile()` 等价于 `bInitialized && Profiles.size() > 1`。
- [x] manager 不缓存第二份可修改的 `SMappingProfile`；Runtime snapshot 仍由 `RuntimeHost` 持有。

### 4.4 Private Helpers

实现以下职责明确的私有辅助函数；名称可按项目风格微调，但不得把逻辑复制到 `ZAppController`：

```cpp
TResult<void> EnsureProfilesDirectory();
TResult<void> DiscoverProfiles();
TResult<StdString> ReadActiveProfileId() const;
TResult<void> WriteActiveProfileId(StdStringView ProfileId) const;
TResult<SLoadedProfile> LoadManagedProfile(const SProfileInfo& Info) const;
TOptional<uint32> FindProfileIndex(StdStringView ProfileId) const;
TResult<StdString> NormalizeAndValidateName(
    StdStringView Name,
    StdStringView IgnoredProfileId = {}) const;
void SortProfiles();
```

- [x] `LoadManagedProfile()` 加载后强制把返回 snapshot 的 `Id/Name` 设为 `SProfileInfo` 的权威值。
- [x] `SaveActiveProfile()` 同样先复制 snapshot，再覆盖其 `Id/Name`，防止 UI 映射编辑意外改变身份字段。
- [x] 文件系统操作使用 `std::error_code` overload，错误转换为现有 `FileReadFailed`、`FileWriteFailed` 或 `InvalidArgument`。
- [x] 比较 profile ID 使用精确匹配；比较显示名使用 ASCII 大小写无关比较，中文等非 ASCII 内容保持原值。
- [x] 名称规范化只移除首尾空白，不修改中间空白，不执行 Unicode NFC/NFKC 正规化。

### 4.5 `Initialize()` Sequence

按以下固定顺序实现：

1. [ ] 校验目录非空并创建目录。
2. [ ] 清空旧的列表和当前 ID，设置 `active_profile.txt` 路径。
3. [ ] 枚举直属 `.json` 文件，按路径排序后逐个调用现有 `LoadProfile()`。
4. [ ] 为缺失 ID/名称的旧文件填入内存 fallback，检查 ID 和名称重复后生成 `SProfileInfo`。
5. [ ] 若列表为空，创建 `Default` JSON 并加入列表。
6. [ ] 排序列表。
7. [ ] 读取 `active_profile.txt`；选出记录项、`default` 或首项。
8. [ ] 完整加载选中项；成功后写入当前 ID 文件。
9. [ ] 最后设置 `ActiveProfileId`、`bInitialized = true` 并返回 `SLoadedProfile`。

初始化中途失败不得留下 `bInitialized == true` 的半初始化状态。

### 4.6 `ActivateProfile()` Sequence

1. [ ] 空 ID、未知 ID 返回 `InvalidArgument`。
2. [ ] ID 等于当前项时仍从当前文件加载并返回，但不重写状态文件。
3. [ ] 加载目标 JSON，验证成功后写 `active_profile.txt`。
4. [ ] 状态文件写入成功后再提交 `ActiveProfileId`。
5. [ ] 返回目标 `SLoadedProfile`；任何失败都保持原 ID 和列表不变。

### 4.7 `CreateProfile()` Sequence

1. [ ] 从 `N = 1` 开始，查找显示名 `Untitled_N`、ID `untitled_N` 和路径 `untitled_N.json` 均未占用的第一个 N。
2. [ ] 构造 `SchemaVersion = 1`、`Id = untitled_N`、`Name = Untitled_N`、`bEnabled = true`、空规则列表的 profile。
3. [ ] 调用现有 `SaveProfile()` 写入新文件。
4. [ ] 写入 `active_profile.txt`；失败时删除本次刚创建的文件并返回错误。
5. [ ] 将描述加入列表并重新排序，提交 `ActiveProfileId`。
6. [ ] 返回新 profile，不复制当前配置的映射。

删除配置后允许复用空出的最小编号；`Untitled_N` 是默认显示名，不承担单调序列号或历史身份语义。

### 4.8 `RenameActiveProfile()` Sequence

1. [ ] 校验 manager 已初始化，规范化新名称，排除当前 ID 后检查重名。
2. [ ] 复制 `CurrentSnapshot`，强制写入当前 ID 和新名称。
3. [ ] 保存到当前 `SProfileInfo::FilePath`。
4. [ ] 保存成功后更新列表中当前项名称并重新排序。
5. [ ] 返回更新后的描述和 snapshot；失败时列表、Runtime snapshot 和文件中的旧名称保持原状。

### 4.9 `DeleteActiveProfile()` Sequence

1. [ ] 配置数量小于等于 1 时返回 `InvalidArgument`。
2. [ ] 从排序列表中选择第一个非当前项作为回退项并完整加载。
3. [ ] 先把 `active_profile.txt` 写为回退 ID。
4. [ ] 使用当前 `SProfileInfo::FilePath` 调用单文件 `std::filesystem::remove`。
5. [ ] 删除失败时尝试把状态文件恢复为原 ID，然后返回错误；内存列表和当前 ID不变。
6. [ ] 删除成功后从列表移除旧项、提交回退 ID，并返回已预加载的回退 profile。

只删除 manager 枚举并保存的精确 JSON 路径；不根据显示名生成删除目标，也不递归删除目录。

## 5. `ZAppController` Implementation

目标文件：

- `source/UI/Bridge/AppController.h`
- `source/UI/Bridge/AppController.cpp`
- `tests/UI/Bridge/AppControllerTests.cpp`
- `tests/UI/QmlSmokeTests.cpp`

### 5.1 Members And Construction

- [x] 增加 `ZProfileManager ProfileManager;`。
- [x] 增加 `StdPath ProfileDirectoryOverride;`，仅供测试构造注入隔离目录。
- [x] 删除 `CachedProfilePath`；活动路径由 `ProfileManager.GetActiveProfileInfo()` 提供。
- [x] 保留 `CachedProfileMessage`、`bProfileDirty`、`CachedProfileSaveState`。
- [x] 保留现有生产构造和两工厂测试构造，额外增加目录可注入重载，避免修改无关测试：

```cpp
ZAppController(
    TInputBackendFactory InputFactory,
    TOutputBackendFactory OutputFactory,
    StdPath ProfileDirectory,
    QObject* Parent = nullptr);
```

- [x] 生产目录辅助函数改为：

```cpp
NODISCARD StdPath ResolveProfilesDirectory() const;
```

有 override 时返回 override，否则返回 `QStandardPaths::AppDataLocation/profiles`。

### 5.2 QML Properties

增加并实现：

```cpp
Q_PROPERTY(QVariantList profileEntries
    READ ProfileEntries NOTIFY profileListChanged)
Q_PROPERTY(QString activeProfileId
    READ ActiveProfileId NOTIFY activeProfileChanged)
Q_PROPERTY(bool canDeleteProfile
    READ CanDeleteProfile NOTIFY profileListChanged)
```

对应 getter：

```cpp
NODISCARD QVariantList ProfileEntries() const;
NODISCARD QString ActiveProfileId() const;
NODISCARD bool CanDeleteProfile() const;
```

- [x] `ProfileEntries()` 将每个 `SProfileInfo` 转成 `{ "id": QString, "name": QString }` 的 `QVariantMap`。
- [x] `activeProfileName` 的 NOTIFY 从 `runtimeStatusChanged` 改为 `activeProfileChanged`，名称从 manager 当前描述读取。
- [x] `ProfilePath()` 从 manager 当前描述读取；尚未初始化时返回空字符串。
- [x] `ProfileDisplayText()` 继续组合当前名称与 `(unsaved)/(save error)`。
- [x] `profileListChanged` 同时通知 `profileEntries` 与 `canDeleteProfile`。
- [x] 核对 QML 绑定：新增 `ProfileSelector` 直接绑定 `activeProfileName/activeProfileChanged`；`outputDisplayText`、`runtimeDisplayText` 等运行时属性继续使用 `runtimeStatusChanged`；`ProfileDisplayText()` 继续由每次活动项变化时同步发出的 `profileStatusChanged` 刷新。

增加信号：

```cpp
void profileListChanged();
void activeProfileChanged();
```

### 5.3 QML Commands

用集合化入口替换 UI Bridge 当前的任意路径 load/save API：

```cpp
Q_INVOKABLE bool initializeProfiles();
Q_INVOKABLE bool saveActiveProfile();
Q_INVOKABLE bool switchProfile(QString ProfileId);
Q_INVOKABLE bool createProfile();
Q_INVOKABLE bool renameActiveProfile(QString NewName);
Q_INVOKABLE bool deleteActiveProfile();
```

- [x] 从 `ZAppController` 移除 `loadProfile(QString)` 与带路径参数的 `saveActiveProfile(QString)`。
- [x] 显式路径的编解码和测试继续直接使用 `ZProfileManager::LoadProfile/SaveProfile`。
- [x] `ZApplicationBootstrap::SApplicationBootstrapOptions::ProfilePath` 保持不变，不参与桌面端配置列表状态。

### 5.4 Shared Controller Helpers

增加：

```cpp
bool ApplyLoadedProfile(
    SLoadedProfile Loaded,
    const QString& SuccessMessage,
    bool bListChanged);
bool SaveDirtyProfileBeforeSelectionChange();
void SetProfileOperationError(const QString& Message);
```

`ApplyLoadedProfile()` 固定执行：

1. [ ] `RuntimeHost::ReplaceProfile(std::move(Loaded.Profile))`。
2. [ ] `RefreshMappingRuleModelFromHost()`。
3. [ ] 设置 `bProfileDirty = false`、`CachedProfileSaveState = "clean"` 和成功消息。
4. [ ] 如列表变化则发 `profileListChanged()`。
5. [ ] 发 `activeProfileChanged()` 和 `profileStatusChanged()`。

`ApplyLoadedProfile()` 不发 `profileLoaded()` 或 `profileSaved()`；这两个操作语义信号由各顶层命令按 5.9 的矩阵显式发出，避免 create 与 rename 共用 helper 时产生歧义。

`bProfileDirty` 是唯一 dirty 权威来源，不与磁盘做内容比对。`SetProfileOperationError()` 固定更新 `CachedProfileMessage`、将保存状态设为 `error`、发 `profileStatusChanged()`，再走现有 `EmitRuntimeError()`，但绝不修改 `bProfileDirty`：映射变更在保存前已由 `MarkProfileDirty()` 设为 true；切换、加载或删除失败则保留操作前的 true/false。

### 5.5 Startup Sequence

`ui/Main.qml` 的 `Component.onCompleted` 改为：

```text
initializeRuntime()
    -> initializeProfiles()
        -> startRuntime()
            -> startPumpTimer(16)
```

`initializeProfiles()`：

1. [ ] 只允许 Bootstrap 为 `Ready` 或 `Running`。
2. [ ] 调用 `ProfileManager.Initialize(ResolveProfilesDirectory())`。
3. [ ] 成功后调用 `ApplyLoadedProfile(..., "Profile loaded", true)`。
4. [ ] 显式发出一次 `profileLoaded(ProfilePath())`。
5. [ ] 失败时保持 Bootstrap 已创建的空 `Default` snapshot，记录错误并返回 false，阻止本次启动 Runtime。

### 5.6 Save And Switch

`saveActiveProfile()`：

1. [ ] 获取 RuntimeHost 当前 snapshot。
2. [ ] 调用 `ProfileManager.SaveActiveProfile()`。
3. [ ] 成功后设为 clean，更新消息，发 `profileStatusChanged/profileSaved`。
4. [ ] 失败后保留 dirty，设为 error 并发出错误。

`SaveDirtyProfileBeforeSelectionChange()`：

- [x] `bProfileDirty == false` 时直接返回 true。
- [x] dirty 时在 manager 的 `ActiveProfileId` 尚未变化时调用 `saveActiveProfile()`，因此只写切换前活动项的 `SProfileInfo::FilePath`；失败返回 false。

`switchProfile(ProfileId)`：

1. [ ] 校验非空；等于当前 ID 时直接返回 true。
2. [ ] 调用 `SaveDirtyProfileBeforeSelectionChange()`，失败时中止。
3. [ ] 调用 `ProfileManager.ActivateProfile()`。
4. [ ] 成功后调用 `ApplyLoadedProfile(..., "Profile loaded", false)`。
5. [ ] 显式发出一次 `profileLoaded(ProfilePath())`。
6. [ ] 不调用 `stopRuntime()`、`startRuntime()`、`stopPumpTimer()` 或 `startPumpTimer()`。

保存旧项和激活新项是严格串行的两个阶段：`SaveActiveProfile()` 返回成功之前不得调用 `ActivateProfile()`；`ActivateProfile()` 提交新 ID 之前不得更新 Runtime 或 QML 当前项。

### 5.7 Create, Rename And Delete

`createProfile()`：

1. [ ] 先保存 dirty 当前项，失败则中止。
2. [ ] 调用 `ProfileManager.CreateProfile()`。
3. [ ] 调用 `ApplyLoadedProfile(..., "Profile created", true)`。
4. [ ] 显式各发一次 `profileSaved(newPath)` 和 `profileLoaded(newPath)`，日志包含新名称。

`renameActiveProfile(NewName)`：

1. [ ] 在 Qt 边界调用 `trimmed()`；空字符串立即报错。
2. [ ] 获取 Runtime 当前 snapshot，调用 manager 的 rename API。
3. [ ] 成功后调用 `ApplyLoadedProfile(..., "Profile renamed", true)`。
4. [ ] `activeProfileChanged()` 即使 ID 未变也必须发出，以刷新名称。
5. [ ] 显式发出一次 `profileSaved(currentPath)`，不发 `profileLoaded()`。

`deleteActiveProfile()`：

1. [ ] `CanDeleteActiveProfile() == false` 时拒绝。
2. [ ] 先保存 dirty 当前项，失败则中止。
3. [ ] 记录将被删除的名称和路径，调用 manager delete API。
4. [ ] 成功后调用 `ApplyLoadedProfile(..., "Profile deleted", true)`。
5. [ ] 显式发出一次回退项的 `profileLoaded(ProfilePath())`。
6. [ ] 日志写明已删除名称和新活动名称。

### 5.8 Existing Mapping Autosave

- [x] `MarkProfileDirty()` 保持现有同步信号行为。
- [x] `AutosaveActiveProfile()` 改为调用无参数 `saveActiveProfile` 的内部实现，不再从 `CachedProfilePath` 或默认文件名推导路径。
- [x] `applySelectedBinding()`、`removeBinding()`、`setBindingEnabled()` 的 Runtime 更新顺序保持不变。
- [x] 切换配置后，上述三个操作只能写 manager 当前项的 `FilePath`。
- [x] `ReplaceActiveProfileForTest()` 保留为纯测试入口，不更新 manager 或磁盘。

### 5.9 Signal Matrix

| 操作 | `profileListChanged` | `activeProfileChanged` | `profileStatusChanged` | `profileSaved` | `profileLoaded` |
|---|---:|---:|---:|---:|---:|
| 初始化成功 | 1 | 1 | 1 | 0 | 1 |
| 切换成功 | 0 | 1 | 1 | 0 | 1 |
| 新建成功 | 1 | 1 | 1 | 1 | 1 |
| 重命名成功 | 1 | 1 | 1 | 1 | 0 |
| 删除成功 | 1 | 1 | 1 | 0 | 1 |
| 映射自动保存成功 | 0 | 0 | 1 | 1 | 0 |
| 任一操作失败 | 0 | 0 | 1 | 0 | 0 |

信号次数按单次顶层操作计算；内部辅助函数不得重复发出同类信号。

## 6. `ProfileSelector.qml` Implementation

目标文件：

- 新增 `ui/panels/ProfileSelector.qml`
- 修改 `ui/panels/TopBar.qml`
- 修改 `ui/Main.qml`
- 修改 `CMakeLists.txt`

### 6.1 Component API

```qml
Item {
    required property var theme
    required property var appController
}
```

组件内部仅保留交互状态：

```qml
property bool renaming: false
property string renameDraft: ""
```

配置列表、当前 ID、当前名称和删除能力均直接绑定 `appController`。

### 6.2 Layout

- [x] 根项高度 `30`。
- [x] 左侧 Profile field 高度 `30`、宽度 `220`，显示 `Profile: <name>` 和向下箭头。
- [x] 右侧依次放置宽度约 `30 / 70 / 60` 的 `+ / Rename / Delete` 按钮，间距沿用 TopBar。
- [x] `TopBar.qml` 用 `ProfileSelector` 替换当前 `Tag`，并设置足够宽度；TopBar 其余标题布局不变。
- [x] 长名称使用 `Text.ElideRight`，不改变 TopBar 高度。
- [x] clean/dirty/error 继续使用当前 `profileSaveSeverity` 映射出的背景或边框颜色。

### 6.3 Dropdown

- [x] `ProfileSelector.qml` 导入 `QtQuick.Controls`，使用 `Popup` 获得 Escape 和点击外部关闭行为；内容仍使用项目现有颜色、Rectangle、Text 和 MouseArea。
- [x] Popup 宽度不小于 Profile field，最大高度 `280`；内容超过最大高度时使用 `ListView` 滚动。
- [x] model 直接绑定 `appController.profileEntries`，delegate 读取 `modelData.id/name`。
- [x] 当前项左侧显示 `✓`，其他项保留同宽空位以保证文字对齐。
- [x] 点击当前项只关闭 Popup；点击其他项调用 `switchProfile(modelData.id)`，仅成功时关闭。
- [x] Popup 每次打开时定位到当前项。
- [x] `renaming == true` 时禁止打开 Popup。

### 6.4 Create

- [x] `+` 点击后先关闭 Popup，再调用 `appController.createProfile()`。
- [x] 成功后的名称和列表完全依赖 controller 信号刷新；QML 不自行追加 `Untitled_N`。

### 6.5 Rename

- [x] 点击 `Rename` 时关闭 Popup，设置 `renameDraft = activeProfileName`、`renaming = true`，下一事件循环让 `TextInput` 获取焦点并 `selectAll()`。
- [x] 重命名状态用 `TextInput` 替代 Profile 文本，不改变 field 尺寸。
- [x] Enter 和焦点离开调用同一个 `commitRename()`。
- [x] `commitRename()` 调用 `renameActiveProfile(renameDraft)`；成功后退出编辑态，失败后保持编辑态并重新获取焦点。
- [x] Escape 调用 `cancelRename()`，恢复当前权威名称且不调用 C++。
- [x] 防止 Enter 导致 `editingFinished` 再次提交：`commitRename()` 开头先检查并设置一个组件内提交 guard，调用结束后清理。

### 6.6 Delete

- [x] `Delete.enabled` 绑定 `appController.canDeleteProfile && !renaming`。
- [x] 点击后关闭 Popup，直接调用 `deleteActiveProfile()`；本轮不增加确认对话框。
- [x] 删除失败时维持当前显示，由 controller 的错误日志和保存状态反馈。

### 6.7 QML Registration

- [x] `CMakeLists.txt` 为 `ui/panels/ProfileSelector.qml` 设置 `QT_RESOURCE_ALIAS ProfileSelector.qml`。
- [x] 将该文件加入 `qt_add_qml_module(MappyZQml ... QML_FILES ...)`。
- [x] `find_package(Qt6 ...)` 增加 `QuickControls2`。
- [x] `MappyZQml` 和 `MappyZQmlSmokeTests` 链接 `Qt6::QuickControls2`，确保开发和部署环境都包含 `QtQuick.Controls`。
- [x] 现有 Windows post-build 会复制 Qt 的完整 `qml` 与 `plugins` 目录；验收复制后的产物中存在 `Qt6/qml/QtQuick/Controls` 及所需 style/plugin，不能只验证 IDE 内运行。
- [x] `ui/Main.qml` 将启动调用从 `loadProfile()` 改为 `initializeProfiles()`。

## 7. Tray Menu Implementation

目标文件：

- `source/App/SystemTrayController.h`
- `source/App/SystemTrayController.cpp`
- `source/App/Main.cpp`
- 新增 `tests/App/SystemTrayControllerTests.cpp`
- 修改 `tests/App/DesktopTestsMain.cpp`
- 修改 `CMakeLists.txt`

### 7.1 Controller API

在 `SystemTrayController.h` 增加：

```cpp
#include <QString>
#include <QVariantList>

void SetProfiles(
    const QVariantList& ProfileEntries,
    const QString& ActiveProfileId);

signals:
    void ProfileSwitchRequested(QString ProfileId);
```

增加成员：

```cpp
QActionGroup* ProfileActionGroup = nullptr;
QList<QAction*> ProfileActions;
QAction* SeparatorAction = nullptr;
```

### 7.2 Menu Rebuild

- [x] 构造函数只创建 `QSystemTrayIcon`、空 `QMenu`，随后调用共用 helper 建立末尾 `Exit`。
- [x] `SetProfiles()` 删除上一批 profile actions 和 separator，然后在 `ExitAction` 前按传入顺序插入新 actions。
- [x] 每个 profile action：
  - [x] text 使用 entry 的 `name`。
  - [x] `setData(entry["id"])`。
  - [x] `setCheckable(true)`。
  - [x] 加入 exclusive `QActionGroup`。
  - [x] ID 等于当前 ID 时 `setChecked(true)`。
  - [x] `objectName` 设置为 `profileAction_<id>`，供测试定位。
- [x] action triggered 时只发一次 `ProfileSwitchRequested(action->data().toString())`。
- [x] separator 始终位于 profile actions 和 `Exit` 之间。
- [x] `Exit` 始终只有一个，保留现有“第一次触发后禁用并发出 `ExitRequested`”逻辑。
- [x] 空列表只显示 `Exit`，不插入 separator；正常应用初始化成功后列表至少有一项。

### 7.3 `Main.cpp` Wiring

在 `Engine.loadFromModule()` 前建立：

```cpp
auto SyncTrayProfiles = [&Tray, &AppController]()
{
    Tray.SetProfiles(
        AppController.ProfileEntries(),
        AppController.ActiveProfileId());
};
```

- [x] `profileListChanged` 和 `activeProfileChanged` 都连接到 `SyncTrayProfiles`。
- [x] `ProfileSwitchRequested` 连接到 lambda：调用 `AppController.switchProfile(ProfileId)`；失败时显式调用 `SyncTrayProfiles()`，恢复 QActionGroup 因点击产生的临时勾选。
- [x] 成功切换依赖 `activeProfileChanged` 自动同步，不在 lambda 中重复刷新。
- [x] QML 根对象成功创建后再显式调用一次 `SyncTrayProfiles()`，覆盖启动信号时序差异。
- [x] `Tray.Show()` 仍位于 QML 根对象创建和首次 `SyncTrayProfiles()` 之后；配置尚未初始化时 `SetProfiles()` 只生成 `Exit`，因此不存在用户可点击但 controller 尚未就绪的 profile action。
- [x] 现有 Restore、Exit、Show/Hide 和窗口生命周期连接保持原顺序。

## 8. Test Implementation

### 8.1 Runtime: `ProfileManagerTests.cpp`

所有集合测试使用各自唯一临时目录，不读取或删除真实 AppData。

- [x] 空目录初始化创建 `Default/default.json/active_profile.txt`，返回空规则 snapshot。
- [x] 旧 `default.json` 缺失 ID 时被发现，返回 snapshot ID 为 `default`，已有规则保持不变。
- [x] 重建 manager 后恢复 `active_profile.txt` 指向的配置。
- [x] 状态文件为空、指向不存在项时按 default/首项规则恢复。
- [x] 配置按名称和 ID稳定排序。
- [x] 损坏 JSON、重复 ID、重复名称不进入列表且原文件仍存在。
- [x] `CreateProfile()` 连续产生 `Untitled_1`、`Untitled_2`；删除 `Untitled_1` 后再次创建可复用编号 1。
- [x] 新建 profile 是空配置，不复制当前规则。
- [x] `SaveActiveProfile()` 只改当前文件，并强制保留 manager 的 ID/名称。
- [x] `ActivateProfile()` 成功更新状态文件；目标丢失/损坏时当前 ID 不变。
- [x] `RenameActiveProfile()` 保存新名称但保持 ID、路径和规则。
- [x] 空白名和忽略大小写重名返回错误，文件不变。
- [x] `DeleteActiveProfile()` 删除精确文件并返回第一项回退 snapshot。
- [x] 最后一项不可删除。
- [x] 删除文件失败时列表和当前 ID 不变。
- [x] 现有 parse/serialize/load/save 测试保持通过。

### 8.2 UI Bridge: `AppControllerTests.cpp`

- [x] 新增测试统一使用 profile directory 注入构造，避免各 case 共享状态。
- [x] `initializeProfiles()` 前 Runtime 未初始化时失败且不创建目录。
- [x] 初始化成功后 `profileEntries/activeProfileId/activeProfileName/profilePath/canDeleteProfile` 一致。
- [x] `ProfileEntries()` 的每项只有 `id/name` 且顺序与 manager 一致。
- [x] 创建成功应用空 Runtime snapshot，并按信号矩阵发信号。
- [x] 切换成功替换 Runtime snapshot 和 `MappingRuleModel`，Runtime 状态及 pump timer 不变。
- [x] dirty 状态切换时先把修改写入旧活动文件，再加载新文件；新文件不得收到旧配置的修改。
- [x] 切换未知/损坏目标时 Runtime snapshot、列表和 active ID 不变。
- [x] 当前 dirty 且保存失败时，切换、新建、删除都被阻止。
- [x] 加载、切换或删除失败只改变错误消息/显示状态，`bProfileDirty` 必须保持操作前的值。
- [x] 重命名成功同步 Runtime snapshot 名称、列表、显示文字和文件内容。
- [x] 删除成功删除旧文件并应用回退 snapshot；只有一项时拒绝。
- [x] 切换后 apply/remove/enable 只改变新活动文件，其他 JSON 字节内容不变。
- [x] 所有成功和失败路径符合信号矩阵，不重复发信号。
- [x] 删除原有依赖 AppController 任意路径 load/save 的测试；等价文件 I/O 覆盖留在 `ProfileManagerTests`。

### 8.3 QML Smoke

- [x] `QmlSmokeTests` 使用 `QTemporaryDir` 和目录注入构造 controller，避免污染真实配置。
- [x] 加载完整 `Main.qml`，确认新增 Controls import、属性和 invokable 无 warning。
- [x] 通过根对象查找 ProfileSelector，验证默认名称、三个按钮和下拉 delegate 可创建。
- [x] 至少覆盖进入/取消 rename 状态，确认无 binding loop 或重复提交 warning。

### 8.4 Desktop / Tray

- [x] `MappyZDesktopTests` 的入口从 `QGuiApplication` 改为 `QApplication`；现有窗口生命周期测试继续原样运行。
- [x] 测试 target 增加 `source/App/SystemTrayController.cpp`、`tests/App/SystemTrayControllerTests.cpp` 和 `Qt6::Widgets`。
- [x] offscreen 下只检查 `QMenu/QAction` 对象树，不要求 `IsAvailable()` 为 true，也不显示原生菜单。
- [x] 验证 profile action 顺序、文本、data、exclusive checked 状态和 separator/Exit 位置。
- [x] 触发非当前 action 只发一次 `ProfileSwitchRequested` 且携带正确 ID。
- [x] 两次 `SetProfiles()` 后旧 actions 已移除，重命名文字已更新，Exit 仍只有一个。
- [x] 现有 Exit 单次发射、Restore 和 WindowLifecycle 测试继续通过。

## 9. Implementation Order

1. [ ] 扩展 `ZProfileManager` 数据结构和 API，以临时目录测试锁定文件语义。
2. [ ] 将 `ZAppController` 切换到集合 API，完成目录注入、属性、命令、信号和 autosave 改造。
3. [ ] 更新 `Main.qml` 启动流程并实现 `ProfileSelector.qml`。
4. [ ] 扩展托盘菜单并在 `Main.cpp` 接通同一 controller 状态。
5. [ ] 更新 CMake、QML smoke 和 Desktop tests。
6. [ ] 执行 Debug 配置、构建、全部自动化测试和 Windows 手动验收。

## 10. Manual Acceptance

- [ ] 首次启动创建并显示 `Default`；重启后仍选中退出前的配置。
- [ ] 点击 Profile 展开全部配置，当前项左侧有勾，选择其他项后映射和顶部名称立即切换。
- [ ] 点击 `+` 依次创建并切换到 `Untitled_1`、`Untitled_2`，每项有独立 JSON。
- [ ] 新配置为空，不包含上一个配置的映射。
- [ ] Rename 后可以直接输入；Enter/失焦提交，Escape 取消；空名和重名不会覆盖文件。
- [ ] Delete 删除当前 JSON 并切换到剩余配置；只剩一项时按钮禁用。
- [ ] 托盘配置顺序与主窗口一致，当前项有勾；窗口隐藏时点击托盘项可以切换。
- [ ] 主窗口创建、切换、重命名、删除后托盘立即同步；托盘切换后主窗口立即同步。
- [ ] 人为制造托盘切换失败后，临时勾选恢复到原活动配置。
- [ ] 切换期间 Runtime 和 pump 保持运行，切换后的映射编辑只写当前配置。
- [ ] 损坏、保存失败、加载失败或删除失败会记录错误，当前可用配置不会被错误替换。
- [ ] 从 post-build 部署目录直接启动可执行文件，Profile 下拉框可打开且无 `QtQuick.Controls`/style/plugin 缺失错误。
- [ ] `Exit`、托盘恢复窗口、关闭到托盘和最小化到托盘行为不回归。

## 11. Definition Of Done

- [x] `ZProfileManager` 是配置目录、配置列表、活动 ID 和配置文件生命周期的唯一来源。
- [x] `ZAppController` 是 Runtime、QML 与托盘之间唯一的配置协调入口。
- [x] 新增、切换、重命名、删除、自动保存和当前配置恢复符合本文顺序与失败语义。
- [x] 现有 `default.json` 无损进入多配置管理。
- [x] 主窗口与托盘始终展示同一列表和当前项。
- [x] Debug 构建通过。
- [x] Runtime、UI Bridge、QML Smoke、Desktop 自动化测试全部通过。
- [ ] Windows 手动验收全部通过。
