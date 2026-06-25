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
