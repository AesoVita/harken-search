// test_undo_system.cpp — Unit tests for UndoStack mechanics (F-025)
// Self-contained: embeds UndoStack implementation to avoid Database link dependency.
#include <cstdio>
#include <string>
#include <memory>
#include <deque>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <chrono>

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

// --- Minimal types from UndoSystem.h (avoid pulling Database) ---

class Database; // forward decl only

enum class UndoOpType { MetadataPatch };

class UndoOperation {
public:
    virtual ~UndoOperation() = default;
    virtual std::string description() const = 0;
    virtual UndoOpType type() const = 0;
    virtual bool undo(Database& db) = 0;
    virtual bool redo(Database& db) = 0;
    int64_t timestamp = 0;
};

static int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class UndoStack {
public:
    static constexpr int kMaxDepth = 50;
    void push(std::unique_ptr<UndoOperation> op) {
        if (!op) return;
        op->timestamp = NowMs();
        m_redoStack.clear();
        m_undoStack.push_back(std::move(op));
        while ((int)m_undoStack.size() > kMaxDepth)
            m_undoStack.pop_front();
    }
    bool undo(Database& db) {
        if (m_undoStack.empty()) return false;
        auto op = std::move(m_undoStack.back());
        m_undoStack.pop_back();
        bool ok = op->undo(db);
        if (ok) m_redoStack.push_back(std::move(op));
        return ok;
    }
    bool redo(Database& db) {
        if (m_redoStack.empty()) return false;
        auto op = std::move(m_redoStack.back());
        m_redoStack.pop_back();
        bool ok = op->redo(db);
        if (ok) m_undoStack.push_back(std::move(op));
        return ok;
    }
    bool canUndo() const { return !m_undoStack.empty(); }
    bool canRedo() const { return !m_redoStack.empty(); }
    std::string undoDescription() const {
        return m_undoStack.empty() ? "" : m_undoStack.back()->description();
    }
    std::string redoDescription() const {
        return m_redoStack.empty() ? "" : m_redoStack.back()->description();
    }
    void clear() { m_undoStack.clear(); m_redoStack.clear(); }
    int undoCount() const { return (int)m_undoStack.size(); }
    int redoCount() const { return (int)m_redoStack.size(); }
private:
    std::deque<std::unique_ptr<UndoOperation>> m_undoStack;
    std::deque<std::unique_ptr<UndoOperation>> m_redoStack;
};

// --- Mock operation ---

static int g_undoCount = 0;
static int g_redoCount = 0;

class MockOp : public UndoOperation {
    std::string m_desc;
public:
    explicit MockOp(const std::string& desc) : m_desc(desc) {}
    std::string description() const override { return m_desc; }
    UndoOpType type() const override { return UndoOpType::MetadataPatch; }
    bool redo(Database&) override { ++g_redoCount; return true; }
    bool undo(Database&) override { ++g_undoCount; return true; }
};

int main() {
    std::printf("=== UndoStack Tests ===\n");

    Database* fakeDb = nullptr;

    // ── Empty stack ──
    {
        UndoStack us;
        CHECK(!us.canUndo(), "empty stack: canUndo false");
        CHECK(!us.canRedo(), "empty stack: canRedo false");
        CHECK(us.undoCount() == 0, "empty stack: undoCount = 0");
        CHECK(us.redoCount() == 0, "empty stack: redoCount = 0");
        CHECK(us.undoDescription().empty(), "empty stack: undoDescription empty");
        CHECK(us.redoDescription().empty(), "empty stack: redoDescription empty");
    }

    // ── Push and undo ──
    {
        UndoStack us;
        g_undoCount = 0;
        us.push(std::make_unique<MockOp>("op1"));
        CHECK(us.canUndo(), "after push: canUndo true");
        CHECK(!us.canRedo(), "after push: canRedo false");
        CHECK(us.undoCount() == 1, "after push: undoCount = 1");
        CHECK(us.undoDescription() == "op1", "undoDescription = op1");

        bool ok = us.undo(*fakeDb);
        CHECK(ok, "undo returns true");
        CHECK(g_undoCount == 1, "undo called revert once");
        CHECK(!us.canUndo(), "after undo: canUndo false");
        CHECK(us.canRedo(), "after undo: canRedo true");
        CHECK(us.redoDescription() == "op1", "redoDescription = op1");
    }

    // ── Redo ──
    {
        UndoStack us;
        g_undoCount = 0;
        g_redoCount = 0;
        us.push(std::make_unique<MockOp>("op2"));
        us.undo(*fakeDb);
        bool ok = us.redo(*fakeDb);
        CHECK(ok, "redo returns true");
        CHECK(g_redoCount == 1, "redo called execute once");
        CHECK(us.canUndo(), "after redo: canUndo true");
        CHECK(!us.canRedo(), "after redo: canRedo false");
    }

    // ── Push clears redo stack ──
    {
        UndoStack us;
        us.push(std::make_unique<MockOp>("a"));
        us.push(std::make_unique<MockOp>("b"));
        us.undo(*fakeDb);
        CHECK(us.canRedo(), "redo available before new push");
        us.push(std::make_unique<MockOp>("c"));
        CHECK(!us.canRedo(), "new push clears redo stack");
        CHECK(us.undoDescription() == "c", "top of stack is 'c'");
    }

    // ── Multiple operations ──
    {
        UndoStack us;
        us.push(std::make_unique<MockOp>("first"));
        us.push(std::make_unique<MockOp>("second"));
        us.push(std::make_unique<MockOp>("third"));
        CHECK(us.undoCount() == 3, "3 operations pushed");
        CHECK(us.undoDescription() == "third", "top is third");

        us.undo(*fakeDb);
        CHECK(us.undoDescription() == "second", "after 1 undo: top is second");
        us.undo(*fakeDb);
        CHECK(us.undoDescription() == "first", "after 2 undo: top is first");
        CHECK(us.redoCount() == 2, "2 items in redo stack");
    }

    // ── Clear ──
    {
        UndoStack us;
        us.push(std::make_unique<MockOp>("x"));
        us.push(std::make_unique<MockOp>("y"));
        us.undo(*fakeDb);
        us.clear();
        CHECK(!us.canUndo(), "after clear: canUndo false");
        CHECK(!us.canRedo(), "after clear: canRedo false");
        CHECK(us.undoCount() == 0, "after clear: undoCount 0");
        CHECK(us.redoCount() == 0, "after clear: redoCount 0");
    }

    // ── Max depth (kMaxDepth = 50) ──
    {
        UndoStack us;
        for (int i = 0; i < 60; i++) {
            us.push(std::make_unique<MockOp>("op" + std::to_string(i)));
        }
        CHECK(us.undoCount() <= UndoStack::kMaxDepth, "stack respects kMaxDepth");
    }

    std::printf("\n=== UndoStack: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
