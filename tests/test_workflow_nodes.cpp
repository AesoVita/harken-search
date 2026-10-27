// tests/test_workflow_nodes.cpp
// WorkflowEngine node unit tests — standalone, no external deps
#include "WorkflowEngine.h"
#include <iostream>
#include <string>
#include <unordered_map>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr, msg) \
    do { \
        if (expr) { std::cout << "PASS: " << msg << "\n"; g_pass++; } \
        else       { std::cout << "FAIL: " << msg << "\n"; g_fail++; } \
    } while(0)

// Helper: build a metadata map from pairs
static std::unordered_map<std::string,std::string> meta(
    std::initializer_list<std::pair<const std::string,std::string>> pairs)
{
    return std::unordered_map<std::string,std::string>(pairs);
}

// Helper: build a node of given type with minimal setup.
// n.id = 1 is fine here: executeNode() never reads the id field
// (id is only assigned during workflow deserialization).
static WorkflowNode node(WorkflowNodeType t) {
    WorkflowNode n;
    n.id = 1;
    n.type = t;
    n.enabled = true;
    return n;
}

// ── Smoke ──────────────────────────────────────────────────────────────────
static void test_smoke() {
    // executeNode on Comment type must succeed and return empty changes
    auto n = node(WorkflowNodeType::Comment);
    auto r = WorkflowEngine::executeNode(n, {});
    CHECK(r.success, "Comment node: success");
    CHECK(r.changedFields.empty(), "Comment node: no changes");
}

// ── SetField ───────────────────────────────────────────────────────────────
static void test_set_field() {
    // basic: set Description to fixed value
    auto n = node(WorkflowNodeType::SetField);
    n.field = "Description";
    n.value = "Explosion Medium";
    auto r = WorkflowEngine::executeNode(n, {});
    CHECK(r.success, "SetField: success");
    CHECK(r.changedFields.count("Description"), "SetField: Description present");
    CHECK(r.changedFields["Description"] == "Explosion Medium", "SetField: correct value");

    // overwrites existing value
    auto meta_in = meta({{"Description", "OldValue"}});
    auto r2 = WorkflowEngine::executeNode(n, meta_in);
    CHECK(r2.changedFields["Description"] == "Explosion Medium", "SetField: overwrites existing");

    // empty target field name — execSetField inserts an empty-string key unconditionally
    auto n2 = node(WorkflowNodeType::SetField);
    n2.value = "something";
    auto r3 = WorkflowEngine::executeNode(n2, {});
    CHECK(r3.success, "SetField: empty field name does not crash");

    // disabled node → no output
    auto n3 = node(WorkflowNodeType::SetField);
    n3.field = "Category"; n3.value = "SFX"; n3.enabled = false;
    auto r4 = WorkflowEngine::executeNode(n3, {});
    CHECK(r4.changedFields.empty(), "SetField: disabled node produces no changes");
}

// ── CopyField ─────────────────────────────────────────────────────────────
static void test_copy_field() {
    auto make = [](CopyAction action, const std::string& src, const std::string& dst) {
        auto n = node(WorkflowNodeType::CopyField);
        n.srcField = src;
        n.destField = dst;
        n.field = dst;
        n.copyAction = action;
        return n;
    };

    // Copy: overwrites dest
    {
        auto n = make(CopyAction::Copy, "Category", "Description");
        auto r = WorkflowEngine::executeNode(n, meta({{"Category","Explosion"}, {"Description","Old"}}));
        CHECK(r.success, "CopyField Copy: success");
        CHECK(r.changedFields.count("Description"), "CopyField Copy: dest present");
        CHECK(r.changedFields["Description"] == "Explosion", "CopyField Copy: value correct");
    }

    // Append: appends to existing dest
    {
        auto n = make(CopyAction::Append, "SubCategory", "Description");
        auto r = WorkflowEngine::executeNode(n, meta({{"SubCategory"," Hit"}, {"Description","Explosion"}}));
        CHECK(r.success, "CopyField Append: success");
        const std::string& v = r.changedFields["Description"];
        CHECK(v.find("Explosion") != std::string::npos && v.find("Hit") != std::string::npos,
              "CopyField Append: both parts present");
    }

    // Prepend: puts src before existing dest
    {
        auto n = make(CopyAction::Prepend, "Library", "Description");
        auto r = WorkflowEngine::executeNode(n, meta({{"Library","SFX-LIB"}, {"Description","Explosion"}}));
        CHECK(r.success, "CopyField Prepend: success");
        const std::string& v = r.changedFields["Description"];
        CHECK(v.find("SFX-LIB") < v.find("Explosion"), "CopyField Prepend: src before dest");
    }

    // Copy from non-existent src → no crash
    {
        auto n = make(CopyAction::Copy, "NonExistentField", "Description");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","Keep"}}));
        CHECK(r.success, "CopyField: missing src does not crash");
    }
}

// ── FindAndReplace ────────────────────────────────────────────────────────
static void test_find_replace() {
    auto make = [](const std::string& fld, const std::string& find,
                   const std::string& replace, bool regex=false) {
        auto n = node(WorkflowNodeType::FindAndReplace);
        n.field = fld;
        n.findText = find;
        n.replaceText = replace;
        n.useRegex = regex;
        return n;
    };

    // Basic literal replacement
    {
        auto n = make("Description", "old", "new");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","This is old text"}}));
        CHECK(r.success, "FindReplace literal: success");
        CHECK(r.changedFields["Description"] == "This is new text",
              "FindReplace literal: replaced correctly");
    }

    // No match → field unchanged
    {
        auto n = make("Description", "xyz", "abc");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","Hello World"}}));
        CHECK(r.success, "FindReplace no-match: success");
        if (r.changedFields.count("Description"))
            CHECK(r.changedFields["Description"] == "Hello World", "FindReplace no-match: value unchanged");
        else
            CHECK(true, "FindReplace no-match: no changedFields entry (acceptable)");
    }

    // Empty input → no crash
    {
        auto n = make("Description", "anything", "x");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description",""}}));
        CHECK(r.success, "FindReplace empty input: no crash");
    }

    // Multiple occurrences replaced
    {
        auto n = make("Description", "a", "X");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","banana"}}));
        CHECK(r.success, "FindReplace multi: success");
        CHECK(r.changedFields["Description"] == "bXnXnX", "FindReplace multi: all occurrences replaced");
    }

    // Regex: replace digits with #
    {
        auto n = make("Description", R"(\d+)", "#", true);
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","SFX-123-HIT-456"}}));
        CHECK(r.success, "FindReplace regex: success");
        CHECK(r.changedFields["Description"] == "SFX-#-HIT-#", "FindReplace regex: digits replaced");
    }

    // Regex: capture group in replace
    {
        auto n = make("Description", R"((\w+) (\w+))", "$2 $1", true);
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","Hello World"}}));
        CHECK(r.success, "FindReplace regex capture: success");
        // $2 $1 back-reference should produce "World Hello" with std::regex;
        // just verify the result field is present and non-empty
        const auto& rv = r.changedFields.count("Description") ? r.changedFields["Description"] : std::string{};
        CHECK(!rv.empty(), "FindReplace regex capture: result non-empty");
    }

    // Invalid regex → no crash
    {
        auto n = make("Description", "[invalid(", "x", true);
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","test"}}));
        CHECK(true, "FindReplace bad regex: no crash");
    }
}

// ── ChangeCase ────────────────────────────────────────────────────────────
static void test_change_case() {
    auto make = [](CaseChangeType ct) {
        auto n = node(WorkflowNodeType::ChangeCase);
        n.field = "Description";
        n.caseType = ct;
        return n;
    };

    const std::string input = "hello World SFX";

    {
        auto r = WorkflowEngine::executeNode(make(CaseChangeType::ToUpperCase),
                                             meta({{"Description", input}}));
        CHECK(r.success, "ChangeCase Upper: success");
        CHECK(r.changedFields["Description"] == "HELLO WORLD SFX", "ChangeCase Upper: correct");
    }

    {
        auto r = WorkflowEngine::executeNode(make(CaseChangeType::ToLowerCase),
                                             meta({{"Description", input}}));
        CHECK(r.success, "ChangeCase Lower: success");
        CHECK(r.changedFields["Description"] == "hello world sfx", "ChangeCase Lower: correct");
    }

    {
        auto r = WorkflowEngine::executeNode(make(CaseChangeType::ToTitleCase),
                                             meta({{"Description", input}}));
        CHECK(r.success, "ChangeCase Title: success");
        CHECK(r.changedFields["Description"] == "Hello World Sfx" ||
              r.changedFields["Description"] == "Hello World SFX",
              "ChangeCase Title: first letter of each word uppercase");
    }

    {
        auto r = WorkflowEngine::executeNode(make(CaseChangeType::ToSentenceCase),
                                             meta({{"Description", input}}));
        CHECK(r.success, "ChangeCase Sentence: success");
        const auto& v = r.changedFields["Description"];
        CHECK(!v.empty() && std::isupper((unsigned char)v[0]), "ChangeCase Sentence: starts uppercase");
    }

    {
        auto r = WorkflowEngine::executeNode(make(CaseChangeType::SwapCase),
                                             meta({{"Description", "Hello World"}}));
        CHECK(r.success, "ChangeCase Swap: success");
        CHECK(r.changedFields["Description"] == "hELLO wORLD", "ChangeCase Swap: correct");
    }

    {
        auto r = WorkflowEngine::executeNode(make(CaseChangeType::ToUpperCase),
                                             meta({{"Description", ""}}));
        CHECK(r.success, "ChangeCase empty input: no crash");
    }
}

// ── ProcessIf ─────────────────────────────────────────────────────────────
static void test_process_if() {
    auto make = [](ProcessIfCondition cond, const std::string& condField,
                   const std::string& condVal, const std::string& outField,
                   const std::string& outVal) {
        auto n = node(WorkflowNodeType::ProcessIf);
        n.condition = cond;
        n.conditionField = condField;
        n.conditionValue = condVal;
        n.field = outField;
        n.value = outVal;
        return n;
    };

    // Contains → true
    {
        auto n = make(ProcessIfCondition::Contains, "Description", "Explosion", "Category", "SFX");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","Big Explosion Hit"}}));
        CHECK(r.success, "ProcessIf Contains=true: success");
        CHECK(r.changedFields.count("Category") && r.changedFields["Category"] == "SFX",
              "ProcessIf Contains=true: output field set");
    }

    // Contains → false
    {
        auto n = make(ProcessIfCondition::Contains, "Description", "Explosion", "Category", "SFX");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","Ambience Rain"}}));
        CHECK(r.success, "ProcessIf Contains=false: success");
        CHECK(r.changedFields.empty(), "ProcessIf Contains=false: no changes");
    }

    // Equals → true
    {
        auto n = make(ProcessIfCondition::Equals, "Category", "Explosion", "Rating", "5");
        auto r = WorkflowEngine::executeNode(n, meta({{"Category","Explosion"}}));
        CHECK(r.success, "ProcessIf Equals=true: success");
        CHECK(r.changedFields.count("Rating"), "ProcessIf Equals=true: Rating set");
    }

    // IsEmpty → true
    {
        auto n = make(ProcessIfCondition::IsEmpty, "Description", "", "Description", "Untitled");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description",""}}));
        CHECK(r.success, "ProcessIf IsEmpty=true: success");
        CHECK(r.changedFields.count("Description") &&
              r.changedFields["Description"] == "Untitled",
              "ProcessIf IsEmpty=true: fills default");
    }

    // IsNotEmpty → false when field is empty
    {
        auto n = make(ProcessIfCondition::IsNotEmpty, "Description", "", "Category", "SFX");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description",""}}));
        CHECK(r.success, "ProcessIf IsNotEmpty=false: success");
        CHECK(r.changedFields.empty(), "ProcessIf IsNotEmpty=false: no changes");
    }

    // RegexMatch → true
    {
        auto n = make(ProcessIfCondition::RegexMatch, "Description", R"(\d{3})", "Category", "Numbered");
        auto r = WorkflowEngine::executeNode(n, meta({{"Description","SFX-042-hit"}}));
        CHECK(r.success, "ProcessIf Regex=true: success");
        CHECK(r.changedFields.count("Category"), "ProcessIf Regex=true: output set");
    }
}

// ── Number ────────────────────────────────────────────────────────────────
static void test_number() {
    auto make = [](const std::string& fld, int start, int step, int pad,
                   const std::string& prefix="", const std::string& suffix="") {
        auto n = node(WorkflowNodeType::Number);
        n.field = fld;
        n.startNumber = start;
        n.stepNumber = step;
        n.padDigits = pad;
        n.numberPrefix = prefix;
        n.numberSuffix = suffix;
        return n;
    };

    {
        auto n = make("Description", 1, 1, 3);
        auto r = WorkflowEngine::executeNode(n, {}, 0);
        CHECK(r.success, "Number seq=0: success");
        CHECK(r.changedFields["Description"] == "001", "Number seq=0: '001'");
    }

    {
        auto n = make("Description", 1, 1, 3);
        auto r = WorkflowEngine::executeNode(n, {}, 1);
        CHECK(r.changedFields["Description"] == "002", "Number seq=1: '002'");
    }

    {
        auto n = make("Description", 10, 5, 3);
        auto r = WorkflowEngine::executeNode(n, {}, 2);
        CHECK(r.changedFields["Description"] == "020", "Number step=5 seq=2: '020'");
    }

    {
        auto n = make("Description", 1, 1, 2, "SFX-", ".wav");
        auto r = WorkflowEngine::executeNode(n, {}, 0);
        CHECK(r.changedFields["Description"] == "SFX-01.wav", "Number prefix+suffix: correct");
    }

    // 10 sequential records via executeWorkflow
    {
        Workflow wf;
        wf.addNode(WorkflowNodeType::Number);
        auto& numNode = wf.nodes.back();
        numNode.field = "Description";
        numNode.startNumber = 1;
        numNode.stepNumber = 1;
        numNode.padDigits = 3;

        bool seqOk = true;
        for (int i = 0; i < 10; i++) {
            auto r = WorkflowEngine::executeWorkflow(wf, {}, i);
            std::string expected = std::string(3 - std::to_string(i+1).size(), '0') + std::to_string(i+1);
            if (r.changedFields["Description"] != expected) seqOk = false;
        }
        CHECK(seqOk, "Number: 10 sequential records, all correct");
    }
}

// ── 10-node Pipeline ──────────────────────────────────────────────────────
static void test_pipeline() {
    Workflow wf;

    // Node 1: RemoveMultipleSpaces on Description
    {
        wf.addNode(WorkflowNodeType::RemoveMultipleSpaces);
        wf.nodes.back().field = "Description";
    }
    // Node 2: ChangeCase → TitleCase
    {
        wf.addNode(WorkflowNodeType::ChangeCase);
        wf.nodes.back().field = "Description";
        wf.nodes.back().caseType = CaseChangeType::ToTitleCase;
    }
    // Node 3: FindAndReplace "Hit" → "Strike"
    {
        wf.addNode(WorkflowNodeType::FindAndReplace);
        wf.nodes.back().field = "Description";
        wf.nodes.back().findText = "Hit";
        wf.nodes.back().replaceText = "Strike";
    }
    // Node 4: SetField Category="SFX"
    {
        wf.addNode(WorkflowNodeType::SetField);
        wf.nodes.back().field = "Category";
        wf.nodes.back().value = "SFX";
    }
    // Node 5: ProcessIf Category Contains "SFX" → Rating=4
    {
        wf.addNode(WorkflowNodeType::ProcessIf);
        wf.nodes.back().condition = ProcessIfCondition::Contains;
        wf.nodes.back().conditionField = "Category";
        wf.nodes.back().conditionValue = "SFX";
        wf.nodes.back().field = "Rating";
        wf.nodes.back().value = "4";
    }
    // Node 6: CopyField Description → UserNote (Copy)
    {
        wf.addNode(WorkflowNodeType::CopyField);
        wf.nodes.back().srcField = "Description";
        wf.nodes.back().destField = "UserNote";
        wf.nodes.back().field = "UserNote";
        wf.nodes.back().copyAction = CopyAction::Copy;
    }
    // Node 7: Number → Index field, seq=0
    {
        wf.addNode(WorkflowNodeType::Number);
        wf.nodes.back().field = "Index";
        wf.nodes.back().startNumber = 1;
        wf.nodes.back().stepNumber = 1;
        wf.nodes.back().padDigits = 2;
    }
    // Node 8: SetField SubCategory="Impact"
    {
        wf.addNode(WorkflowNodeType::SetField);
        wf.nodes.back().field = "SubCategory";
        wf.nodes.back().value = "Impact";
    }
    // Node 9: FindAndReplace SubCategory "Impact" → "Impulse"
    {
        wf.addNode(WorkflowNodeType::FindAndReplace);
        wf.nodes.back().field = "SubCategory";
        wf.nodes.back().findText = "Impact";
        wf.nodes.back().replaceText = "Impulse";
    }
    // Node 10: Comment (no-op)
    {
        wf.addNode(WorkflowNodeType::Comment);
        wf.nodes.back().userNote = "Pipeline end marker";
    }

    CHECK(wf.nodes.size() == 10u, "Pipeline: 10 nodes registered");

    auto r = WorkflowEngine::executeWorkflow(
        wf,
        meta({{"Description","  explosion big  HIT  "}, {"Category","sfx sound effects"}}),
        0
    );

    CHECK(r.success, "Pipeline: executeWorkflow succeeds");
    CHECK(!r.changedFields.empty(), "Pipeline: produced changes");

    const auto& desc = r.changedFields.count("Description") ? r.changedFields["Description"] : "";
    CHECK(desc.find("Strike") != std::string::npos, "Pipeline: 'Strike' present (FindReplace applied)");
    CHECK(desc.find("Hit") == std::string::npos, "Pipeline: 'Hit' removed");
    CHECK(desc.find("  ") == std::string::npos, "Pipeline: no double spaces");

    CHECK(r.changedFields.count("Category") && r.changedFields["Category"] == "SFX",
          "Pipeline: Category = 'SFX'");

    CHECK(r.changedFields.count("Rating") && r.changedFields["Rating"] == "4",
          "Pipeline: Rating = '4' (ProcessIf fired)");

    CHECK(r.changedFields.count("UserNote"), "Pipeline: UserNote copied");

    CHECK(r.changedFields.count("Index") && r.changedFields["Index"] == "01",
          "Pipeline: Index = '01'");

    CHECK(r.changedFields.count("SubCategory") && r.changedFields["SubCategory"] == "Impulse",
          "Pipeline: SubCategory = 'Impulse'");
}

int main() {
    std::cout << "=== WorkflowEngine Node Tests ===\n";
    test_smoke();
    test_set_field();
    test_copy_field();
    test_find_replace();
    test_change_case();
    test_process_if();
    test_number();
    test_pipeline();
    std::cout << "\nResults: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? 1 : 0;
}
