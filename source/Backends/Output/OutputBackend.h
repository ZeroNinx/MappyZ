// 输出后端统一接口。
// 所有输出后端（Windows SendInput、Null 后端等）都必须实现此接口。
// Runtime 通过此接口发送动作，不直接依赖具体后端实现。
//
// 本接口只依赖 Core 层数据类型，不引入 Qt、SDL 或平台 API。

#pragma once

#include "Core/Action.h"
#include "Core/ProjectCore.h"

namespace MappyZ
{

// 输出后端运行状态
enum class EOutputBackendState
{
    Unavailable,  // 后端不可用（未初始化、缺少权限等）
    Ready,        // 后端已就绪，可以发送动作
    Error,        // 后端遇到错误，需要恢复
};

// 输出后端状态快照，供 Runtime 和 UI 观察
struct SOutputBackendStatus
{
    EOutputBackendState State = EOutputBackendState::Unavailable;
    StdString Message;
};

class IOutputBackend
{
public:
    virtual ~IOutputBackend() = default;

    // 禁止拷贝和移动，后端持有内部状态，所有权语义不适合拷贝
    IOutputBackend(const IOutputBackend&) = delete;
    IOutputBackend& operator=(const IOutputBackend&) = delete;
    IOutputBackend(IOutputBackend&&) = delete;
    IOutputBackend& operator=(IOutputBackend&&) = delete;

    // 发送单个动作到输出端。失败时通过 TResult 返回错误信息。
    NODISCARD virtual TResult<void> SendAction(const SAction& Action) = 0;

    // 查询后端当前状态快照
    NODISCARD virtual SOutputBackendStatus GetStatus() const = 0;

protected:
    // 只允许子类构造
    IOutputBackend() = default;
};

}  // namespace MappyZ
