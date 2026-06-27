// ZMappingRuleModel 单元测试。
// 验证 role 映射、ReplaceRules、clear、ruleIdAt、
// 以及 output/actionKind 从 SAction payload 提取逻辑。

#include <catch2/catch_test_macros.hpp>

#include "Core/Action.h"
#include "Core/MappingRule.h"
#include "UI/Bridge/MappingRuleModel.h"

using namespace MappyZ;

// ── 构造辅助 ──

static SMappingRule MakeKeyboardRule(
    const StdString& ControlId,
    const StdString& Key)
{
    SMappingRule Rule;
    Rule.Id = ControlId;
    Rule.DisplayName = ControlId;
    Rule.bEnabled = true;
    Rule.Input.ControlId = ControlId;
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::KeyboardKey;
    Rule.Output.Action.Payload = SKeyboardAction{.Key = Key, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    return Rule;
}

static SMappingRule MakeMouseButtonRule(
    const StdString& ControlId,
    int32 Button)
{
    SMappingRule Rule;
    Rule.Id = ControlId;
    Rule.DisplayName = ControlId;
    Rule.bEnabled = true;
    Rule.Input.ControlId = ControlId;
    Rule.Input.ControlType = EInputControlType::Button;
    Rule.Input.EventType = EInputEventType::Pressed;
    Rule.Output.Action.Type = EActionType::MouseButton;
    Rule.Output.Action.Payload = SMouseButtonAction{.Button = Button, .bPressed = true};
    Rule.Output.Mode = EMappingActionMode::PressRelease;
    return Rule;
}

// ── 默认状态 ──

TEST_CASE("MappingRuleModel default is empty",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;
    REQUIRE(Model.rowCount() == 0);
}

// ── ReplaceRules ──

TEST_CASE("MappingRuleModel ReplaceRules populates model",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Rules.push_back(MakeMouseButtonRule("right_trigger", 0));

    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.rowCount() == 2);
}

// ── Role 语义：input 返回原始 controlId ──

TEST_CASE("MappingRuleModel input role returns raw controlId",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Model.ReplaceRules(std::move(Rules));

    auto Index = Model.index(0);
    REQUIRE(Model.data(Index, ZMappingRuleModel::InputRole).toString() == "button_south");
}

// ── Role 语义：output 从 keyboard payload 提取 key ──

TEST_CASE("MappingRuleModel output role extracts keyboard key from payload",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Model.ReplaceRules(std::move(Rules));

    auto Index = Model.index(0);
    REQUIRE(Model.data(Index, ZMappingRuleModel::OutputRole).toString() == "Space");
}

// ── Role 语义：output 从 mouse payload 提取按钮名 ──

TEST_CASE("MappingRuleModel output role extracts mouse button name from payload",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeMouseButtonRule("right_trigger", 0));
    Model.ReplaceRules(std::move(Rules));

    auto Index = Model.index(0);
    REQUIRE(Model.data(Index, ZMappingRuleModel::OutputRole).toString() == "Left Click");
}

// ── Role 语义：actionKind 从 action type 推导 ──

TEST_CASE("MappingRuleModel actionKind role derives from action type",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Rules.push_back(MakeMouseButtonRule("right_trigger", 0));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::ActionKindRole).toString()
        == "Keyboard");
    REQUIRE(Model.data(Model.index(1), ZMappingRuleModel::ActionKindRole).toString()
        == "MouseButton");
}

// ── Role 语义：ruleId 返回 Rule.Id ──

TEST_CASE("MappingRuleModel ruleId role returns rule id",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Model.ReplaceRules(std::move(Rules));

    auto Index = Model.index(0);
    REQUIRE(Model.data(Index, ZMappingRuleModel::RuleIdRole).toString() == "button_south");
}

// ── Role 语义：enabled 返回 bEnabled ──

TEST_CASE("MappingRuleModel enabled role returns bEnabled",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Model.ReplaceRules(std::move(Rules));

    auto Index = Model.index(0);
    REQUIRE(Model.data(Index, ZMappingRuleModel::EnabledRole).toBool() == true);
}

// ── clear ──

TEST_CASE("MappingRuleModel clear empties model",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Model.ReplaceRules(std::move(Rules));
    REQUIRE(Model.rowCount() == 1);

    Model.clear();
    REQUIRE(Model.rowCount() == 0);
}

// ── clear 重复调用安全 ──

TEST_CASE("MappingRuleModel repeated clear is safe",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;
    Model.clear();
    Model.clear();
    REQUIRE(Model.rowCount() == 0);
}

// ── ruleIdAt ──

TEST_CASE("MappingRuleModel ruleIdAt returns id for valid row",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Rules.push_back(MakeKeyboardRule("button_north", "Escape"));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.ruleIdAt(0) == "button_south");
    REQUIRE(Model.ruleIdAt(1) == "button_north");
}

// ── ruleIdAt 越界返回空串 ──

TEST_CASE("MappingRuleModel ruleIdAt out of range returns empty",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;
    REQUIRE(Model.ruleIdAt(0) == "");
    REQUIRE(Model.ruleIdAt(-1) == "");
}

// ── ListRulesSnapshot ──

TEST_CASE("MappingRuleModel ListRulesSnapshot returns copy",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Model.ReplaceRules(std::move(Rules));

    auto Snapshot = Model.ListRulesSnapshot();
    REQUIRE(Snapshot.size() == 1);
    REQUIRE(Snapshot[0].Id == "button_south");

    // 修改 snapshot 不影响 model
    Snapshot.clear();
    REQUIRE(Model.rowCount() == 1);
}

// ── ReplaceRules 替换旧数据 ──

TEST_CASE("MappingRuleModel ReplaceRules replaces existing data",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules1;
    Rules1.push_back(MakeKeyboardRule("button_south", "Space"));
    Rules1.push_back(MakeKeyboardRule("button_north", "Escape"));
    Model.ReplaceRules(std::move(Rules1));
    REQUIRE(Model.rowCount() == 2);

    TVector<SMappingRule> Rules2;
    Rules2.push_back(MakeMouseButtonRule("right_trigger", 0));
    Model.ReplaceRules(std::move(Rules2));
    REQUIRE(Model.rowCount() == 1);
    REQUIRE(Model.ruleIdAt(0) == "right_trigger");
}

// ── data 越界返回空 QVariant ──

TEST_CASE("MappingRuleModel data out of range returns invalid QVariant",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    auto Index = Model.index(0);
    REQUIRE_FALSE(Model.data(Index, ZMappingRuleModel::RuleIdRole).isValid());
}

// ── mouse button 右键和中键输出名 ──

TEST_CASE("MappingRuleModel mouse button right and middle output names",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeMouseButtonRule("button_east", 1));
    Rules.push_back(MakeMouseButtonRule("button_west", 2));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::OutputRole).toString()
        == "Right Click");
    REQUIRE(Model.data(Model.index(1), ZMappingRuleModel::OutputRole).toString()
        == "Middle Click");
}

// ── roleNames 覆盖 ──

TEST_CASE("MappingRuleModel roleNames contains all expected roles",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;
    auto Names = Model.roleNames();

    REQUIRE(Names.contains(ZMappingRuleModel::RuleIdRole));
    REQUIRE(Names.contains(ZMappingRuleModel::InputRole));
    REQUIRE(Names.contains(ZMappingRuleModel::OutputRole));
    REQUIRE(Names.contains(ZMappingRuleModel::ActionKindRole));
    REQUIRE(Names.contains(ZMappingRuleModel::ActionValueRole));
    REQUIRE(Names.contains(ZMappingRuleModel::DisplayKindRole));
    REQUIRE(Names.contains(ZMappingRuleModel::EnabledRole));

    REQUIRE(Names[ZMappingRuleModel::RuleIdRole] == "ruleId");
    REQUIRE(Names[ZMappingRuleModel::InputRole] == "input");
    REQUIRE(Names[ZMappingRuleModel::OutputRole] == "output");
    REQUIRE(Names[ZMappingRuleModel::ActionKindRole] == "actionKind");
    REQUIRE(Names[ZMappingRuleModel::ActionValueRole] == "actionValue");
    REQUIRE(Names[ZMappingRuleModel::DisplayKindRole] == "displayKind");
    REQUIRE(Names[ZMappingRuleModel::EnabledRole] == "ruleEnabled");
}

// ── actionValue role ──

TEST_CASE("MappingRuleModel actionValue role returns catalog value for keyboard",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::ActionValueRole).toString()
        == "Space");
}

TEST_CASE("MappingRuleModel actionValue role returns catalog value for mouse buttons",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeMouseButtonRule("btn_a", 0));
    Rules.push_back(MakeMouseButtonRule("btn_b", 1));
    Rules.push_back(MakeMouseButtonRule("btn_c", 2));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::ActionValueRole).toString()
        == "Left");
    REQUIRE(Model.data(Model.index(1), ZMappingRuleModel::ActionValueRole).toString()
        == "Right");
    REQUIRE(Model.data(Model.index(2), ZMappingRuleModel::ActionValueRole).toString()
        == "Middle");
}

// ── displayKind role ──

TEST_CASE("MappingRuleModel displayKind role returns UI tag for keyboard and mouse",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeKeyboardRule("button_south", "Space"));
    Rules.push_back(MakeMouseButtonRule("right_trigger", 0));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::DisplayKindRole).toString()
        == "Keyboard");
    REQUIRE(Model.data(Model.index(1), ZMappingRuleModel::DisplayKindRole).toString()
        == "Mouse");
}

// ── MouseMove role 验证 ──

static SMappingRule MakeMouseMoveRule(const StdString& ControlId)
{
    SMappingRule Rule;
    Rule.Id = ControlId;
    Rule.DisplayName = ControlId;
    Rule.bEnabled = true;
    Rule.Input.ControlId = ControlId;
    Rule.Input.ControlType = EInputControlType::Axis2D;
    Rule.Input.EventType = EInputEventType::Changed;
    Rule.Input.Deadzone = 0.20f;
    Rule.Input.Threshold = 0.0f;
    Rule.Output.Action.Type = EActionType::MouseMove;
    Rule.Output.Action.Payload = SMouseMoveAction{.DeltaX = 0.0f, .DeltaY = 0.0f};
    Rule.Output.Mode = EMappingActionMode::Analog;
    Rule.Output.Sensitivity = 12.0f;
    return Rule;
}

TEST_CASE("MappingRuleModel MouseMove output role returns Move Cursor",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeMouseMoveRule("left_stick"));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::OutputRole).toString()
        == "Move Cursor");
}

TEST_CASE("MappingRuleModel MouseMove actionValue role returns Cursor",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeMouseMoveRule("left_stick"));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::ActionValueRole).toString()
        == "Cursor");
}

TEST_CASE("MappingRuleModel MouseMove displayKind role returns Mouse",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeMouseMoveRule("left_stick"));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::DisplayKindRole).toString()
        == "Mouse");
}

TEST_CASE("MappingRuleModel MouseMove actionKind role returns MouseMove",
    "[UI][MappingRuleModel]")
{
    ZMappingRuleModel Model;

    TVector<SMappingRule> Rules;
    Rules.push_back(MakeMouseMoveRule("left_stick"));
    Model.ReplaceRules(std::move(Rules));

    REQUIRE(Model.data(Model.index(0), ZMappingRuleModel::ActionKindRole).toString()
        == "MouseMove");
}
