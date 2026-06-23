// Windows SendInput 输出后端。
// 将 SAction 转换为 Win32 SendInput 调用，产生真实键盘和鼠标输入。
// 头文件不包含 Windows.h，public API 不出现 Win32 类型。
//
// 通过构造注入 TNativeSender 实现测试可控：
// - 默认构造使用真实 SendInput（定义在 .cpp 中）。
// - 测试时注入 fake sender，记录调用参数，不产生系统输入。

#pragma once

#include "Backends/Output/OutputBackend.h"
#include "Backends/Output/WindowsSendInputHelpers.h"

#include <functional>

namespace MappyZ
{

class ZWindowsSendInputBackend final : public IOutputBackend
{
public:
    // native sender 签名：接收 SendInput 命令，返回是否成功
    using TNativeSender = std::function<bool(const SendInputHelpers::SSendInputCommand& Command)>;

    // 使用真实 SendInput 的默认构造
    ZWindowsSendInputBackend();

    // 注入自定义 sender 的测试构造
    explicit ZWindowsSendInputBackend(TNativeSender CustomSender);

    NODISCARD TResult<void> SendAction(const SAction& Action) override;
    NODISCARD SOutputBackendStatus GetStatus() const override;

private:
    TNativeSender NativeSender;
    SOutputBackendStatus CurrentStatus;

    // 鼠标移动小数残差累积，避免小于 1 像素的移动丢失
    float32 MouseResidualX = 0.0f;
    float32 MouseResidualY = 0.0f;
};

}  // namespace MappyZ
