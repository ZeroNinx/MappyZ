# TODO: Real Output Mode

## Next Step

下一步实现“真实输出模式”开关：默认继续使用 `NullOutput`，用户明确确认后才切换到真实输出后端，用于验证 `Apply -> mapping -> dispatch -> system output` 的端到端闭环。

本轮只做单一输出模式切换，不做设置页、不做持久化用户偏好、不做复杂权限检测 UI。

## Scope

包含：

- [x] UI 增加明确的 Real Output 开关或确认按钮。
- [x] `ZAppController` 增加输出模式切换 API。
- [x] 默认启动仍使用 `NullOutput`。
- [x] 用户主动开启真实输出后，重建 runtime host 使用真实输出后端。
- [x] 真实输出不可用或创建失败时，自动回退到 `NullOutput` 并显示错误/提示。
- [x] 切换输出模式时保留当前 active profile snapshot。
- [x] 切换输出模式时保留 `mappingEnabled`。
- [x] 切换输出模式后刷新 `mappingRuleModel` 和 runtime status。
- [x] 增加 UI Bridge / App 层测试覆盖输出模式切换链路。

不做：

- [x] 不保存真实输出偏好到 profile 或配置文件。
- [x] 不启动时自动使用真实输出。
- [x] 不做系统权限弹窗或管理员权限处理。
- [x] 不做 per-profile output mode。
- [x] 不做 SendInput 行为扩展，只使用已有输出后端。
- [x] 不修改 mapping/action 语义。

## API Plan

`ZAppController` 新增：

- [x] `Q_PROPERTY(bool realOutputEnabled READ IsRealOutputEnabled NOTIFY outputModeChanged)`
- [x] `Q_PROPERTY(bool outputModeSwitching READ IsOutputModeSwitching NOTIFY outputModeChanged)`
- [x] `Q_INVOKABLE bool setRealOutputEnabled(bool enabled)`
- [x] `bool IsRealOutputEnabled() const`
- [x] `bool IsOutputModeSwitching() const`
- [x] `void outputModeChanged()`

复用已有属性：

- [x] `outputDisplayText` 继续作为用户可读输出模式展示。
- [x] `outputState` 继续作为底层 backend state 调试属性。
- [x] `runtimeStatusChanged()` 用于刷新 runtime/output/profile 状态。
- [x] `runtimeError(message)` 用于真实输出不可用或切换失败。

`ZApplicationBootstrap` 增加显式重建入口：

- [x] 新增 `TResult<void> Reinitialize(SApplicationBootstrapOptions Options)`，无论当前是 Ready/Running/Error 都执行完整 teardown + setup。
- [x] 保持现有 `Initialize()` 的幂等语义不变，避免破坏已有调用方。
- [x] `Reinitialize()` 内部复用现有 setup 路径，确保 host 先析构、backend 后析构。
- [x] `Reinitialize()` 成功后更新 `CachedOptions`，`IsUsingNullOutput()` 反映实际当前模式。
- [x] `SApplicationBootstrapOptions` 增加 `bool bSkipProfileSetup = false`。
- [x] `Initialize()` 默认保持现有 profile 行为：`ProfilePath` 为空时创建默认 `Default` profile，非空时从文件加载。
- [x] `Reinitialize(... bSkipProfileSetup=true)` 跳过默认 profile 创建和文件加载，避免切换输出模式时先创建空 profile 再立刻被 AppController 覆盖。
- [x] P5 的输出模式切换必须使用 `bSkipProfileSetup=true`，profile 恢复完全由 `ZAppController` 的 snapshot/replace 流程负责。

输出后端创建语义：

- [x] `bUseNullOutput = true` 时强制创建 `ZNullOutputBackend`。
- [x] `bUseNullOutput = false` 时必须尝试真实 output factory。
- [x] 如果当前构建没有真实输出后端，默认 output factory 应返回失败，不再静默返回 `NullOutput`。
- [x] 调整相关现有测试：使用生产默认 factory 的路径若请求真实输出，应按“真实输出不可用则失败”断言，不再期待隐式 NullOutput fallback。
- [x] 回退到 `NullOutput` 的策略放在 `ZAppController::setRealOutputEnabled(true)`，而不是藏在 factory 内部。

## Behavior

- [x] App 启动仍调用 `initializeRuntime(true)`，默认 `NullOutput`。
- [x] 本轮 `setRealOutputEnabled(...)` 是同步 API，前提是假设当前后端创建/销毁是快速同步操作（NullOutput、Fake、Windows SendInput 均应如此）。
- [x] 如果后续引入慢速 I/O 或异步设备初始化，再把输出模式切换重构为异步命令。
- [x] `setRealOutputEnabled(false)`：
  - [x] 如果当前已是 NullOutput，返回 true no-op。
  - [x] 如果当前是真实输出，切换回 NullOutput。
- [x] `setRealOutputEnabled(true)`：
  - [x] 如果当前已是真实输出，返回 true no-op。
  - [x] 弹出/触发 UI 确认后才调用。
  - [x] 尝试重建 runtime 使用真实输出。
  - [x] 真实输出创建失败时，重建回 NullOutput，返回 false，发 `runtimeError()`，UI toggle 保持关闭。
- [x] Runtime 未 initialize 时，`setRealOutputEnabled(...)` 返回 false 并发 `runtimeError()`。
- [x] 切换前保存当前 `RuntimeHost.GetProfileSnapshot()`。
- [x] profile 保留机制由 `ZAppController::setRealOutputEnabled(...)` 负责：snapshot -> reinitialize -> replace profile -> refresh UI。
- [x] 切换成功或回退成功后把保存的 profile snapshot 重新 `ReplaceProfile()` 到新 host。
- [x] 切换前保存 `mappingEnabled`，切换后恢复。
- [x] 切换前如果 runtime 是 Running，切换后应重新 StartRuntime。
- [x] 切换前如果 pump timer 正在运行，切换后应恢复 timer。
- [x] 切换失败但 NullOutput 回退成功时，保持原 profile、mapping list、mapping enabled 和 running/pump 状态。
- [x] 切换期间内部 `bOutputModeSwitching == true`，重复调用 `setRealOutputEnabled(...)` 返回 false 或安全 no-op，不启动第二次重建。
- [x] `outputModeSwitching` 暴露给 QML 作为防御性 busy 绑定；本轮同步实现下 QML 通常只能观测最终 false 状态，不依赖它实现核心正确性。
- [x] 切换输出模式可以清空 runtime input state；这是后端重建后的合理状态。
- [x] 切换输出模式后必须调用 `InputStateModelInstance.clear()`，发出 reset，避免 QML 显示旧按键/轴状态。
- [x] 切换后刷新 `DeviceModel`，以反映重建后输入后端的设备快照。
- [x] 切换后刷新 `MappingRuleModel`。
- [x] 切换成功写入 `Info` 或 `Success` 级日志。
- [x] 真实输出不可用并回退时写入 `Error` 级日志。

## QML Plan

- [x] `Main.qml` 默认仍使用 `initializeRuntime(true)`。
- [x] `TopBar` 增加明确的真实输出开关，和 `Mapping On/Off`、`Save Profile` 同级，默认关闭。
- [x] 开启真实输出前需要可见确认：
  - [x] 本轮使用两步确认按钮，不引入 Dialog 或设置页。
  - [x] 第一次点击显示 `Confirm Real Output`。
  - [x] 3 秒内第二次点击才调用 `appController.setRealOutputEnabled(true)`。
  - [x] 超时后按钮恢复普通 `Real Output` 文案。
- [x] 关闭真实输出不需要确认。
- [x] 开关状态绑定 `appController.realOutputEnabled`。
- [x] 切换过程中按钮绑定 `appController.outputModeSwitching` 禁用或显示 busy 状态，避免重复点击。
- [x] 切换成功后 `StatusBar` 的 `Output:` 通过 `outputDisplayText` 显示 `RealOutput` / `NullOutput`。
- [x] 切换失败后保持 `NullOutput`，并通过 Event Log / runtime error 给用户可见反馈。
- [x] QML 不直接重建 runtime，不直接保存/恢复 profile；这些都由 `ZAppController` 处理。

## Tests

`ApplicationBootstrapTests.cpp`：

- [x] `Initialize(bUseNullOutput=true)` 使用 NullOutput。
- [x] `Initialize(bUseNullOutput=false)` 调用真实 output factory。
- [x] 默认真实 output factory 不可用时返回错误，而不是静默 NullOutput。
- [x] `Reinitialize()` 在 Ready 状态下会重建 output backend。
- [x] `Reinitialize()` 在 Running 状态下安全停止旧 host 并重建。
- [x] `Reinitialize()` 在 Error 状态下也执行完整 setup，并可恢复到 Ready。
- [x] `Reinitialize()` 后旧 host/backend 不再被引用，新 host 使用新 backend。
- [x] `Reinitialize(... bSkipProfileSetup=true)` 不创建默认 profile、不从磁盘加载 profile，由调用方后续 `ReplaceProfile()`。

`AppControllerTests.cpp`：

- [x] 默认 initialize 使用 NullOutput，`realOutputEnabled == false`。
- [x] 默认 `outputModeSwitching == false`。
- [x] `setRealOutputEnabled(true)` 调用真实 output factory。
- [x] 真实输出成功后 `realOutputEnabled == true`，`outputDisplayText == RealOutput`。
- [x] `setRealOutputEnabled(false)` 切回 NullOutput。
- [x] 真实 output factory 失败时返回 false，发 `runtimeError()`，并回退到 NullOutput。
- [x] 切换失败后 runtime 保持 Ready 或 Running 状态，与切换前一致。
- [x] 切换输出模式保留 active profile rules。
- [x] 切换输出模式保留 `activeProfileName`。
- [x] 切换输出模式保留 `mappingEnabled`。
- [x] Running 状态下切换后仍是 Running。
- [x] pump timer active 时切换后仍 active。
- [x] 切换后 `mappingRuleModel` 与 active profile 一致。
- [x] 切换后 `inputStateModel` 被清空并发出 reset。
- [x] 同步实现下同模式重复调用为 no-op（防御性覆盖，非真正中间态防重入）。
- [x] 切换结束后 `outputModeSwitching == false` 并发出 `outputModeChanged()`。
- [x] QML smoke 无 binding/import/property warning。

## Acceptance Criteria

- [x] 默认启动仍是安全的 `NullOutput`。
- [x] 用户必须明确确认才能开启真实输出。
- [x] 真实输出可用时，UI 能切换到 `RealOutput`，StatusBar 正确显示。
- [x] 真实输出不可用时，UI 自动回到 `NullOutput`，并给出错误反馈。
- [x] 切换模式不丢失当前 profile、mapping rules、profile name 或 mapping enabled。
- [x] 切换模式不会把加载/保存逻辑塞进 QML。
- [x] 所有测试通过。
- [x] 修改和新增文本文件使用 CRLF 行尾。

## Follow-Up

- [ ] 后续保存用户上次选择的输出模式偏好。
- [ ] 后续增加更完整的设置页。
- [ ] 后续按平台显示真实输出后端的可用性和权限状态。
