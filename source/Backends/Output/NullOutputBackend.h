// Null 输出后端。
// 只记录动作，不产生系统级输入。用于测试、调试和端到端验证。
// 提供测试注入接口：可切换状态、清空记录、查询动作列表。

#pragma once

#include "Backends/Output/OutputBackend.h"

namespace MappyZ
{

class ZNullOutputBackend final : public IOutputBackend
{
public:
    ZNullOutputBackend();

    NODISCARD TResult<void> SendAction(const SAction& Action) override;
    NODISCARD SOutputBackendStatus GetStatus() const override;

    // ── 查询接口 ──

    // 返回已记录动作的快照拷贝
    NODISCARD TVector<SAction> ListActions() const;

    // 返回当前记录的动作数量
    NODISCARD uint32 GetActionCount() const noexcept;

    // ── 测试注入接口 ──

    // 清空动作记录，不改变状态
    void ClearActions();

    // 设置完整状态快照
    void SetStatus(SOutputBackendStatus Status);

    // 切换到 Ready 状态
    void SetReady(StdString Message = "ready");

    // 切换到 Unavailable 状态
    void SetUnavailable(StdString Message);

    // 切换到 Error 状态
    void SetError(StdString Message);

private:
    SOutputBackendStatus CurrentStatus;
    TVector<SAction> RecordedActions;
};

}  // namespace MappyZ
