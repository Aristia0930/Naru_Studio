#include "TestMain.h"

#include "naru/data/Database.h"
#include "naru/events/EventCompiler.h"
#include "naru/runtime/Session.h"

#include <algorithm>
#include <string>

using namespace naru;

namespace {

Database sampleDatabase() {
    Database db;
    DataType items = makeTemplate(DataTemplate::Item, db);
    items.records = {{"potion", "포션", {{"price", std::int64_t{50}}}}};
    db.types.push_back(items);
    DataType monsters = makeTemplate(DataTemplate::Monster, db);
    monsters.records = {{"slime", "슬라임", {{"drop", std::string("potion")}}}};
    db.types.push_back(monsters);
    return db;
}

bool hasIssue(const std::vector<DataIssue>& issues, const std::string& location, const std::string& text) {
    return std::any_of(issues.begin(), issues.end(), [&](const DataIssue& i) {
        return i.location == location && i.message.find(text) != std::string::npos;
    });
}

FieldDef intField(std::optional<double> min, std::optional<double> max) {
    FieldDef f;
    f.key = "v";
    f.type = FieldType::Integer;
    f.min = min;
    f.max = max;
    return f;
}

} // namespace

TEST_CASE("템플릿은 고칠 수 있는 정의이며 키가 겹치지 않는다") {
    Database db;
    DataType a = makeTemplate(DataTemplate::Item, db);
    CHECK(a.key == "item" && a.name == "아이템" && !a.fields.empty());
    db.types.push_back(a);
    CHECK(makeTemplate(DataTemplate::Item, db).key == "item_2");
    // 아이템 종류가 있으면 몬스터의 드롭이 그것을 참조한다
    DataType m = makeTemplate(DataTemplate::Monster, db);
    CHECK(m.findField("drop") && m.findField("drop")->refType == "item");
    CHECK(makeTemplate(DataTemplate::Monster, Database{}).findField("drop")->refType.empty());
    CHECK(makeTemplate(DataTemplate::Empty, db).fields.empty());
    for (const auto& t : {a, m, makeTemplate(DataTemplate::Character, db)}) {
        for (const auto& f : t.fields) {
            CHECK(isValidFieldKey(f.key));
            CHECK(!checkValue(f.defaultValue, f).has_value());
        }
    }
}

TEST_CASE("저장된 값이 없으면 기본값을 쓴다") {
    Database db = sampleDatabase();
    const DataType* items = db.findType("item");
    const DataRecord* potion = items->findRecord("potion");
    CHECK(items->valueOf(*potion, *items->findField("price")) == Value{std::int64_t{50}});
    CHECK(items->valueOf(*potion, *items->findField("consumable")) == Value{true});
    CHECK(items->newRecordId("item") == "item_001");
}

TEST_CASE("값 검사: 형식·범위·선택지") {
    CHECK(!checkValue(std::int64_t{5}, intField(0, 10)));
    CHECK(checkValue(std::int64_t{11}, intField(0, 10)).value().find("범위") != std::string::npos);
    CHECK(checkValue(2.5, intField({}, {})).has_value());
    FieldDef choice;
    choice.type = FieldType::Choice;
    choice.choices = {"불", "물"};
    CHECK(!checkValue(std::string("물"), choice));
    CHECK(checkValue(std::string("땅"), choice).has_value());
    FieldDef ref;
    ref.type = FieldType::Reference;
    CHECK(!checkValue(Value{}, ref));
    CHECK(checkValue(std::int64_t{1}, ref).has_value());
}

TEST_CASE("속성 형식을 바꿀 때 값 변환") {
    FieldDef number;
    number.type = FieldType::Number;
    CHECK(convertValue(std::int64_t{3}, number) == Value{std::int64_t{3}}); // 이미 맞는 값은 그대로
    CHECK(convertValue(std::string("2.5"), number) == Value{2.5});
    CHECK(!convertValue(std::string("많음"), number).has_value());
    CHECK(convertValue(2.6, intField(0, 10)) == Value{std::int64_t{3}});
    CHECK(convertValue(std::int64_t{50}, intField(0, 10)) == Value{std::int64_t{10}}); // 범위로 맞춘다
    FieldDef text;
    text.type = FieldType::Text;
    CHECK(convertValue(std::int64_t{7}, text) == Value{std::string("7")});
    FieldDef boolean;
    boolean.type = FieldType::Boolean;
    CHECK(convertValue(std::int64_t{0}, boolean) == Value{false});
}

TEST_CASE("검증: 끊긴 참조·정의 안 된 값·범위 밖 값을 경고한다") {
    Database db = sampleDatabase();
    CHECK(db.validate().empty());
    db.findType("monster")->records[0].values["drop"] = std::string("elixir");
    db.findType("monster")->records[0].values["hp"] = std::int64_t{0};
    db.findType("item")->records[0].values["unknown"] = std::int64_t{1};
    auto issues = db.validate();
    CHECK(hasIssue(issues, "monster/slime.drop", "elixir"));
    CHECK(hasIssue(issues, "monster/slime.hp", "범위"));
    CHECK(hasIssue(issues, "item/potion.unknown", "정의되지 않은"));
    CHECK((db.referencesTo("item") == std::vector<std::string>{"monster.drop"}));
}

TEST_CASE("키 규칙") {
    CHECK(isValidDataId("slime_01") && isValidDataId("a-b"));
    CHECK(!isValidDataId("") && !isValidDataId("../x") && !isValidDataId("슬라임"));
    CHECK(isValidFieldKey("hp") && isValidFieldKey("_x1"));
    CHECK(!isValidFieldKey("1hp") && !isValidFieldKey("id") && !isValidFieldKey("name") && !isValidFieldKey("h-p"));
}

TEST_CASE("이벤트의 데이터 → 변수 명령") {
    Database db = sampleDatabase();
    auto getData = [](std::string type, std::string record, std::string field, std::string variable) {
        EventCommand c;
        c.type = EventCommandType::GetData;
        c.dataType = std::move(type);
        c.record = std::move(record);
        c.field = std::move(field);
        c.variable = std::move(variable);
        return c;
    };
    Session session(Variables{});
    session.setDatabase(&db);
    session.start(compileEvent({getData("item", "potion", "price", "p"), getData("monster", "slime", "drop", "d"),
                                getData("monster", "slime", "name", "n"), getData("item", "potion", "heal", "h")},
                               {}));
    CHECK(session.state() == SessionState::Completed);
    CHECK(session.variables().get("p") == Value{std::int64_t{50}});
    CHECK(session.variables().get("d") == Value{std::string("potion")});
    CHECK(session.variables().get("n") == Value{std::string("슬라임")});
    CHECK(session.variables().get("h") == Value{std::int64_t{0}}); // 기본값

    Session missing(Variables{});
    missing.setDatabase(&db);
    missing.start(compileEvent({getData("monster", "dragon", "hp", "x")}, {}));
    CHECK(missing.state() == SessionState::Failed);
    CHECK(missing.runner().failureMessage().find("dragon") != std::string::npos);

    Session noDb(Variables{});
    noDb.start(compileEvent({getData("item", "potion", "price", "x")}, {}));
    CHECK(noDb.state() == SessionState::Failed);
}

TEST_MAIN()
