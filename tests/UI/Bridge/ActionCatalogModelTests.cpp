// ZActionCatalogModel 单元测试。
// 验证 rowCount、role 映射、catalog 内容完整性、越界访问。

#include <catch2/catch_test_macros.hpp>

#include "UI/Bridge/ActionCatalogModel.h"

using namespace MappyZ;

// ── catalog 基本属性 ──

TEST_CASE("[UI][ActionCatalogModel] default rowCount is positive", "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    REQUIRE(Model.rowCount() > 0);
}

TEST_CASE("[UI][ActionCatalogModel] roles contain kind value displayText category",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    auto Roles = Model.roleNames();
    REQUIRE(Roles.contains(ZActionCatalogModel::KindRole));
    REQUIRE(Roles.contains(ZActionCatalogModel::ValueRole));
    REQUIRE(Roles.contains(ZActionCatalogModel::DisplayTextRole));
    REQUIRE(Roles.contains(ZActionCatalogModel::CategoryRole));
    CHECK(Roles[ZActionCatalogModel::KindRole] == "kind");
    CHECK(Roles[ZActionCatalogModel::ValueRole] == "value");
    CHECK(Roles[ZActionCatalogModel::DisplayTextRole] == "displayText");
    CHECK(Roles[ZActionCatalogModel::CategoryRole] == "category");
}

// ── catalog 内容：特殊键 ──

TEST_CASE("[UI][ActionCatalogModel] catalog contains Keyboard Space", "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    bool bFound = false;
    for (int Row = 0; Row < Model.rowCount(); ++Row)
    {
        auto Index = Model.index(Row);
        if (Model.data(Index, ZActionCatalogModel::KindRole) == "Keyboard"
            && Model.data(Index, ZActionCatalogModel::ValueRole) == "Space")
        {
            bFound = true;
            CHECK(Model.data(Index, ZActionCatalogModel::DisplayTextRole).toString()
                == "Keyboard: Space");
            CHECK(Model.data(Index, ZActionCatalogModel::CategoryRole) == "Keyboard");
            break;
        }
    }
    REQUIRE(bFound);
}

TEST_CASE("[UI][ActionCatalogModel] catalog contains Enter Escape Tab",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    QStringList ExpectedValues = {"Enter", "Escape", "Tab"};
    for (const auto& Value : ExpectedValues)
    {
        bool bFound = false;
        for (int Row = 0; Row < Model.rowCount(); ++Row)
        {
            auto Index = Model.index(Row);
            if (Model.data(Index, ZActionCatalogModel::KindRole) == "Keyboard"
                && Model.data(Index, ZActionCatalogModel::ValueRole) == Value)
            {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

// ── catalog 内容：字母 A-Z ──

TEST_CASE("[UI][ActionCatalogModel] catalog contains Keyboard A through Z",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    for (char Letter = 'A'; Letter <= 'Z'; ++Letter)
    {
        QString Value(QChar::fromLatin1(Letter));
        bool bFound = false;
        for (int Row = 0; Row < Model.rowCount(); ++Row)
        {
            auto Index = Model.index(Row);
            if (Model.data(Index, ZActionCatalogModel::KindRole) == "Keyboard"
                && Model.data(Index, ZActionCatalogModel::ValueRole) == Value)
            {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

// ── catalog 内容：数字 0-9 ──

TEST_CASE("[UI][ActionCatalogModel] catalog contains Keyboard 0 through 9",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    for (char Digit = '0'; Digit <= '9'; ++Digit)
    {
        QString Value(QChar::fromLatin1(Digit));
        bool bFound = false;
        for (int Row = 0; Row < Model.rowCount(); ++Row)
        {
            auto Index = Model.index(Row);
            if (Model.data(Index, ZActionCatalogModel::KindRole) == "Keyboard"
                && Model.data(Index, ZActionCatalogModel::ValueRole) == Value)
            {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

// ── catalog 内容：方向键 ──

TEST_CASE("[UI][ActionCatalogModel] catalog contains arrow keys", "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    QStringList ArrowValues = {"ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"};
    for (const auto& Value : ArrowValues)
    {
        bool bFound = false;
        for (int Row = 0; Row < Model.rowCount(); ++Row)
        {
            auto Index = Model.index(Row);
            if (Model.data(Index, ZActionCatalogModel::KindRole) == "Keyboard"
                && Model.data(Index, ZActionCatalogModel::ValueRole) == Value)
            {
                bFound = true;
                break;
            }
        }
        REQUIRE(bFound);
    }
}

// ── catalog 内容：鼠标按钮 ──

TEST_CASE("[UI][ActionCatalogModel] catalog contains MouseButton Left Right Middle",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    QStringList MouseValues = {"Left", "Right", "Middle"};
    for (const auto& Value : MouseValues)
    {
        bool bFound = false;
        for (int Row = 0; Row < Model.rowCount(); ++Row)
        {
            auto Index = Model.index(Row);
            if (Model.data(Index, ZActionCatalogModel::KindRole) == "MouseButton"
                && Model.data(Index, ZActionCatalogModel::ValueRole) == Value)
            {
                bFound = true;
                CHECK(Model.data(Index, ZActionCatalogModel::CategoryRole) == "Mouse");
                break;
            }
        }
        REQUIRE(bFound);
    }
}

TEST_CASE("[UI][ActionCatalogModel] catalog contains MouseButton Button4 and Button5",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    REQUIRE(Model.Contains("MouseButton", "Button4"));
    REQUIRE(Model.Contains("MouseButton", "Button5"));

    int Button4Index = Model.findIndex("MouseButton", "Button4");
    auto Button4ModelIndex = Model.index(Button4Index);
    CHECK(Model.data(Button4ModelIndex, ZActionCatalogModel::CategoryRole) == "Mouse");

    int Button5Index = Model.findIndex("MouseButton", "Button5");
    auto Button5ModelIndex = Model.index(Button5Index);
    CHECK(Model.data(Button5ModelIndex, ZActionCatalogModel::CategoryRole) == "Mouse");
}

// ── kindAt / valueAt / displayTextAt 越界 ──

TEST_CASE("[UI][ActionCatalogModel] kindAt returns empty for out of bounds row",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    CHECK(Model.kindAt(-1).isEmpty());
    CHECK(Model.kindAt(Model.rowCount()).isEmpty());
    CHECK(Model.kindAt(9999).isEmpty());
}

TEST_CASE("[UI][ActionCatalogModel] valueAt returns empty for out of bounds row",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    CHECK(Model.valueAt(-1).isEmpty());
    CHECK(Model.valueAt(Model.rowCount()).isEmpty());
}

TEST_CASE("[UI][ActionCatalogModel] displayTextAt returns empty for out of bounds row",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    CHECK(Model.displayTextAt(-1).isEmpty());
    CHECK(Model.displayTextAt(Model.rowCount()).isEmpty());
}

// ── kindAt / valueAt / displayTextAt 正常访问 ──

TEST_CASE("[UI][ActionCatalogModel] kindAt valueAt displayTextAt return correct values for row 0",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    // 第一条目录项是 Keyboard / Space
    CHECK(Model.kindAt(0) == "Keyboard");
    CHECK(Model.valueAt(0) == "Space");
    CHECK(Model.displayTextAt(0) == "Keyboard: Space");
}

// ── data() 对无效 Index 返回空 QVariant ──

TEST_CASE("[UI][ActionCatalogModel] data returns empty QVariant for invalid index",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    auto Invalid = Model.index(-1);
    CHECK(!Model.data(Invalid, ZActionCatalogModel::KindRole).isValid());
}

// ── data() 对未知 Role 返回空 QVariant ──

TEST_CASE("[UI][ActionCatalogModel] data returns empty QVariant for unknown role",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    auto Index = Model.index(0);
    CHECK(!Model.data(Index, Qt::UserRole + 999).isValid());
}

// ── rowCount: parent 有效时返回 0（flat list 语义）──

TEST_CASE("[UI][ActionCatalogModel] rowCount returns 0 for valid parent",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    auto ParentIndex = Model.index(0);
    CHECK(Model.rowCount(ParentIndex) == 0);
}

// ── findIndex ──

TEST_CASE("[UI][ActionCatalogModel] findIndex returns correct index for existing entry",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;

    int Index = Model.findIndex("Keyboard", "Space");
    REQUIRE(Index >= 0);
    REQUIRE(Model.kindAt(Index) == "Keyboard");
    REQUIRE(Model.valueAt(Index) == "Space");
}

TEST_CASE("[UI][ActionCatalogModel] findIndex returns correct index for mouse button",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;

    int Index = Model.findIndex("MouseButton", "Left");
    REQUIRE(Index >= 0);
    REQUIRE(Model.kindAt(Index) == "MouseButton");
    REQUIRE(Model.valueAt(Index) == "Left");
}

TEST_CASE("[UI][ActionCatalogModel] findIndex returns -1 for nonexistent entry",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;

    REQUIRE(Model.findIndex("Keyboard", "NonExistentKey") == -1);
    REQUIRE(Model.findIndex("UnknownKind", "Space") == -1);
    REQUIRE(Model.findIndex("", "") == -1);
}

// ── catalog 内容：MouseMove ──

TEST_CASE("[UI][ActionCatalogModel] catalog contains MouseMove Cursor",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    REQUIRE(Model.Contains("MouseMove", "Cursor"));
}

TEST_CASE("[UI][ActionCatalogModel] MouseMove display text is Mouse Move Cursor",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    int Index = Model.findIndex("MouseMove", "Cursor");
    REQUIRE(Index >= 0);
    REQUIRE(Model.displayTextAt(Index) == "Mouse: Move Cursor");
}

TEST_CASE("[UI][ActionCatalogModel] MouseMove category is Mouse",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    int Index = Model.findIndex("MouseMove", "Cursor");
    REQUIRE(Index >= 0);
    auto ModelIndex = Model.index(Index);
    REQUIRE(Model.data(ModelIndex, ZActionCatalogModel::CategoryRole).toString()
        == "Mouse");
}

TEST_CASE("[UI][ActionCatalogModel] findIndex MouseMove Cursor returns valid row",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;
    int Index = Model.findIndex("MouseMove", "Cursor");
    REQUIRE(Index >= 0);
    REQUIRE(Model.kindAt(Index) == "MouseMove");
    REQUIRE(Model.valueAt(Index) == "Cursor");
}

// ── 扩展键位覆盖 ──

TEST_CASE("[UI][ActionCatalogModel] contains expanded keyboard keys",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;

    // F 键
    REQUIRE(Model.Contains("Keyboard", "F1"));
    REQUIRE(Model.Contains("Keyboard", "F5"));
    REQUIRE(Model.Contains("Keyboard", "F12"));

    // 编辑/导航键
    REQUIRE(Model.Contains("Keyboard", "Backspace"));
    REQUIRE(Model.Contains("Keyboard", "Delete"));
    REQUIRE(Model.Contains("Keyboard", "Insert"));
    REQUIRE(Model.Contains("Keyboard", "Home"));
    REQUIRE(Model.Contains("Keyboard", "End"));
    REQUIRE(Model.Contains("Keyboard", "PageUp"));
    REQUIRE(Model.Contains("Keyboard", "PageDown"));

    // 修饰键
    REQUIRE(Model.Contains("Keyboard", "LeftShift"));
    REQUIRE(Model.Contains("Keyboard", "RightShift"));
    REQUIRE(Model.Contains("Keyboard", "LeftCtrl"));
    REQUIRE(Model.Contains("Keyboard", "RightCtrl"));
    REQUIRE(Model.Contains("Keyboard", "LeftAlt"));
    REQUIRE(Model.Contains("Keyboard", "RightAlt"));
    REQUIRE(Model.Contains("Keyboard", "LeftMeta"));

    // 符号键
    REQUIRE(Model.Contains("Keyboard", "Minus"));
    REQUIRE(Model.Contains("Keyboard", "Equal"));
    REQUIRE(Model.Contains("Keyboard", "LeftBracket"));
    REQUIRE(Model.Contains("Keyboard", "RightBracket"));
    REQUIRE(Model.Contains("Keyboard", "Backslash"));
    REQUIRE(Model.Contains("Keyboard", "Semicolon"));
    REQUIRE(Model.Contains("Keyboard", "Apostrophe"));
    REQUIRE(Model.Contains("Keyboard", "Comma"));
    REQUIRE(Model.Contains("Keyboard", "Period"));
    REQUIRE(Model.Contains("Keyboard", "Slash"));
    REQUIRE(Model.Contains("Keyboard", "Backquote"));
}

TEST_CASE("[UI][ActionCatalogModel] numpad keys are distinct from main keys",
    "[ActionCatalogModel]")
{
    ZActionCatalogModel Model;

    // 小键盘 Num0-Num9 与主键区 0-9 分开
    REQUIRE(Model.Contains("Keyboard", "Num0"));
    REQUIRE(Model.Contains("Keyboard", "Num1"));
    REQUIRE(Model.Contains("Keyboard", "Num9"));
    REQUIRE(Model.Contains("Keyboard", "NumDivide"));
    REQUIRE(Model.Contains("Keyboard", "NumMultiply"));
    REQUIRE(Model.Contains("Keyboard", "NumSubtract"));
    REQUIRE(Model.Contains("Keyboard", "NumAdd"));
    REQUIRE(Model.Contains("Keyboard", "NumDecimal"));

    // 主键区 "1" 和小键盘 "Num1" 索引不同
    int MainKey1 = Model.findIndex("Keyboard", "1");
    int NumKey1 = Model.findIndex("Keyboard", "Num1");
    REQUIRE(MainKey1 >= 0);
    REQUIRE(NumKey1 >= 0);
    REQUIRE(MainKey1 != NumKey1);
}
