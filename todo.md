# TODO: Action Picker

## Next Step

下一步实现 `Binding Editor` 的 Action Picker：不再用 `"Keyboard: Space"` 这类展示字符串作为业务输入，而是让 UI 从 C++ 提供的 action catalog 中选择结构化 action，再把 `actionKind + actionValue` 传给 `ZAppController` 写入 active profile。

本轮只做常用键盘键和鼠标按钮选择，不做宏、组合键、长按、鼠标移动预设或完整设置页。

## Scope

包含：

- [x] 新增 C++ 侧 action catalog，作为 UI 可选输出动作的单一事实来源。
- [x] `ZAppController` 暴露只读 `actionCatalogModel`。
- [x] `applySelectedBinding(...)` 改为结构化参数，不再解析展示字符串。
- [x] `BindingEditor.qml` 使用 action catalog 渲染选项。
- [x] Apply 后仍写入现有 `SMappingRule` / `SAction`，不改变 Core action 语义。
- [x] Current mappings 仍使用 `mappingRuleModel` 展示已有规则。
- [x] 增加 UI Bridge / QML smoke 测试覆盖结构化 action apply 链路。

不做：

- [x] 不做组合键，例如 `Ctrl+S`。
- [x] 不做宏或多 action sequence。
- [x] 不做 Hold / Toggle / Turbo 模式。
- [x] 不做 MouseMove / MouseWheel 配置 UI。
- [x] 不做 per-device action catalog。
- [x] 不做复杂弹窗或设置页。

## Design Direction

当前字符串解析的问题：

- [x] UI 展示文案和业务语义耦合，后续本地化或文案调整会破坏 apply。
- [x] `"Keyboard: Space"` / `"Mouse: Left Click"` 这种格式不属于 Core contract。
- [x] 测试容易只覆盖展示字符串，而不是 action payload。

本轮改为：

- [x] C++ 提供 action catalog model。
- [x] QML 只保存选中项的 `actionKind` 和 `actionValue`。
- [x] `ZAppController` 根据结构化参数构造 `SAction`。
- [x] 展示文本只用于 UI，不参与业务解析。

## Proposed Types

新增 `source/UI/Bridge/ActionCatalogModel.h/.cpp`：

```cpp
class ZActionCatalogModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum ERole
    {
        KindRole = Qt::UserRole + 1,
        ValueRole,
        DisplayTextRole,
        CategoryRole,
    };

    explicit ZActionCatalogModel(QObject* Parent = nullptr);

    int rowCount(const QModelIndex& Parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& Index, int Role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString kindAt(int row) const;
    Q_INVOKABLE QString valueAt(int row) const;
    Q_INVOKABLE QString displayTextAt(int row) const;
};
```

Internal item shape:

```cpp
struct SActionCatalogItem
{
    QString Kind;        // "Keyboard" / "MouseButton"
    QString Value;       // "Space" / "Enter" / "A" / "Left"
    QString DisplayText; // "Keyboard: Space" / "Mouse: Left Click"
    QString Category;    // "Keyboard" / "Mouse"
};
```

`ZAppController` 新增：

```cpp
Q_PROPERTY(ZActionCatalogModel* actionCatalogModel READ ActionCatalogModel CONSTANT)

NODISCARD ZActionCatalogModel* ActionCatalogModel();
Q_INVOKABLE bool applySelectedBinding(
    QString controlId,
    QString actionKind,
    QString actionValue);
```

`actionKind` 固定集合：

- [x] `"Keyboard"`：写入 `SKeyboardAction`。
- [x] `"MouseButton"`：写入 `SMouseButtonAction`。

`actionValue` 固定集合：

- [x] Keyboard: `Space`、`Enter`、`Escape`、`Tab`。
- [x] Keyboard: `A` 到 `Z`。
- [x] Keyboard: `0` 到 `9`。
- [x] Keyboard: `ArrowUp`、`ArrowDown`、`ArrowLeft`、`ArrowRight`。
- [x] MouseButton: `Left`、`Right`、`Middle`。

Mouse button 映射由 `ZAppController::applySelectedBinding(...)` 内部维护：

- [x] `"Left"` -> `SMouseButtonAction.Button = 0`。
- [x] `"Right"` -> `SMouseButtonAction.Button = 1`。
- [x] `"Middle"` -> `SMouseButtonAction.Button = 2`。

## Behavior

- [x] `actionCatalogModel` 在 `ZAppController` 构造时初始化，运行期不动态变化。
- [x] `BindingEditor` 默认选中 `Keyboard / Space`，保持当前用户体验。
- [x] 用户切换 action option 时，只更新 QML 局部状态：`selectedActionKind` / `selectedActionValue` / `selectedActionDisplayText`。
- [x] Apply 时调用结构化 `applySelectedBinding(selectedControl, selectedActionKind, selectedActionValue)`。
- [x] `applySelectedBinding` 不读取 UI 展示字符串。
- [x] `applySelectedBinding` 对未知 `actionKind` 返回 false，不修改 profile，写 Error log。
- [x] `applySelectedBinding` 对未知 `actionValue` 返回 false，不修改 profile，写 Error log。
- [x] `controlId` 为空时返回 false，不修改 profile。
- [x] Runtime 未 initialize 或 Error 时行为保持 P0 语义：UI 禁用 Apply；C++ 仍防御性返回 false。
- [x] Apply 成功后刷新 `mappingRuleModel`。
- [x] Apply 成功后继续写 Success log。
- [x] Apply 失败后继续写 Error log。
- [x] `SMappingRule::Id` 仍使用 `controlId`，同一 control apply 替换原 rule。
- [x] `SMappingRule::Output.Action.Type` 由 `actionKind` 决定，不由 QML 文案决定。
- [x] Keyboard action 继续使用 `SKeyboardAction.bPressed = true`，与现有 `PressRelease` rule mode 配合生成按下/释放语义。

## QML Plan

- [x] `BindingEditor.qml` 移除硬编码 `Keyboard` / `Mouse` 两个 action 按钮。
- [x] 使用 `appController.actionCatalogModel` 渲染 action choices。
- [x] UI 采用一个轻量 `ComboBox` 或等价下拉选择，不引入弹窗。
- [x] 下拉项使用 flat list，不做 section header；`category` role 本轮只用于后续分组扩展和测试验证。
- [x] 每个 action choice 展示 `displayText`。
- [x] `BindingEditor` 内部保存 `selectedActionKind`、`selectedActionValue`、`selectedActionDisplayText`。
- [x] `Main.qml` 不再持有 `selectedAction: "Keyboard: Space"` 这类 root property；Action 选择状态收敛在 `BindingEditor` 内部。
- [x] `Action output` 显示选中项的 `displayText`。
- [x] Apply 按钮禁用逻辑继续沿用 P0：无设备、无 control、无 action、runtime error 时禁用。
- [x] Capture / Clear 交互不变。
- [x] Current mappings 展示不变，仍由 `mappingRuleModel` 的 `actionKind` / `output` roles 驱动。
- [x] QML 不拼业务 action 字符串。

## Compatibility

- [x] 如果测试或 QML 仍需要旧的 `"Keyboard: Space"` 格式，应在本轮迁移掉，不保留双路径。
- [x] 迁移所有现有 `applySelectedBinding` 调用，从 2 参数改为 3 参数；当前包含约 30 处 `AppControllerTests.cpp` 调用和 1 处 `BindingEditor.qml` 调用。
- [x] 删除旧的展示字符串解析 helper，避免新旧路径并存导致行为分叉。
- [x] 不新增 QML 直接构造 `SAction` 的能力。
- [x] 不让 `MappingRuleModel` 反向承担 action catalog 职责。
- [x] 保存/加载 profile 文件格式不变，因为底层仍是现有 `SMappingProfile`。

## Tests

`ActionCatalogModelTests.cpp`：

- [x] 默认 rowCount 大于 0。
- [x] roles 包含 `kind`、`value`、`displayText`、`category`。
- [x] catalog 包含 `Keyboard / Space`。
- [x] catalog 包含 `Keyboard / A` 到 `Keyboard / Z`。
- [x] catalog 包含 `Keyboard / 0` 到 `Keyboard / 9`。
- [x] catalog 包含 arrow keys。
- [x] catalog 包含 `MouseButton / Left`、`Right`、`Middle`。
- [x] `kindAt/valueAt/displayTextAt` 对越界 row 返回空字符串。

`AppControllerTests.cpp`：

- [x] `actionCatalogModel` 非空且可读。
- [x] `applySelectedBinding(controlId, "Keyboard", "Space")` 写入 `SKeyboardAction`。
- [x] `applySelectedBinding(controlId, "Keyboard", "A")` 写入对应 keyboard action。
- [x] `applySelectedBinding(controlId, "MouseButton", "Left")` 写入 `SMouseButtonAction.Button = 0`。
- [x] `applySelectedBinding(controlId, "MouseButton", "Right")` 写入 `SMouseButtonAction.Button = 1`。
- [x] `applySelectedBinding(controlId, "MouseButton", "Middle")` 写入 `SMouseButtonAction.Button = 2`。
- [x] 未知 `actionKind` 返回 false，不修改现有 profile。
- [x] 未知 `actionValue` 返回 false，不修改现有 profile。
- [x] 空 `controlId` 返回 false。
- [x] Apply 成功刷新 `mappingRuleModel`。
- [x] Apply 成功/失败日志语义保持。
- [x] 旧展示字符串路径被移除，测试不再依赖 `"Keyboard: Space"` 解析。
- [x] 所有旧签名调用点已迁移，代码库中不再存在 `applySelectedBinding(controlId, actionText)` 形式。

QML smoke：

- [x] `BindingEditor` 能绑定 `actionCatalogModel`，无 required property warning。
- [x] 默认选中 action 显示 `Keyboard: Space`。
- [x] Apply 调用结构化参数，不出现展示字符串解析依赖。

## Acceptance Criteria

- [x] UI 能选择 Space / Enter / Escape / Tab / A-Z / 0-9 / Arrow keys / Left-Right-Middle mouse。
- [x] Apply 写入的 action payload 正确。
- [x] 修改 display text 不会改变业务语义。
- [x] 未知 action 被拒绝且不污染 profile。
- [x] Current mappings 继续正确显示。
- [x] 所有测试通过。
- [x] 修改和新增文本文件使用 CRLF 行尾。

## Follow-Up

- [ ] 后续增加 MouseMove / MouseWheel picker。
- [ ] 后续增加组合键和 modifier。
- [ ] 后续增加搜索或分组 action picker UI。
- [ ] 后续增加 per-platform key name 展示。
