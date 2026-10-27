#include "MiniWaveformScheduler.h"
#include "SelectedWaveformRebuildQueue.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_failed = 0;

MiniWaveformTextureKey Key(const char* path, int width, int height)
{
    MiniWaveformTextureKey key;
    key.path = path;
    key.width = width;
    key.height = height;
    return key;
}

MiniWaveformTextureKey Key(std::string path, int width, int height)
{
    MiniWaveformTextureKey key;
    key.path = std::move(path);
    key.width = width;
    key.height = height;
    return key;
}

void Expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << "\n";
        ++g_failed;
    }
}

void TestDedupe()
{
    MiniWaveformScheduler scheduler;

    Expect(scheduler.schedule("a.wav", 96, 24), "first schedule should enqueue");
    Expect(!scheduler.schedule("a.wav", 96, 24), "duplicate schedule should be ignored");
    Expect(scheduler.schedule("a.wav", 128, 24), "different size should enqueue");
    Expect(scheduler.pendingCount() == 2, "dedupe should keep pending count stable");
    Expect(scheduler.contains(Key("a.wav", 96, 24)), "contains should find pending key");
    Expect(scheduler.containsPath("a.wav"), "containsPath should find pending path");
}

void TestFifoPop()
{
    MiniWaveformScheduler scheduler;
    scheduler.schedule("a.wav", 96, 24);
    scheduler.schedule("b.wav", 96, 24);
    scheduler.schedule("c.wav", 96, 24);

    MiniWaveformTextureKey out;
    Expect(scheduler.popNext(out), "first pop should succeed");
    Expect(out == Key("a.wav", 96, 24), "first pop should return first scheduled key");
    Expect(scheduler.popNext(out), "second pop should succeed");
    Expect(out == Key("b.wav", 96, 24), "second pop should return second scheduled key");
    Expect(scheduler.pendingCount() == 1, "pop should remove from pending set");
    Expect(!scheduler.contains(Key("b.wav", 96, 24)), "popped key should no longer be contained");
    Expect(!scheduler.containsPath("b.wav"), "popped path should no longer be contained");
}

void TestScheduleFrontPriority()
{
    MiniWaveformScheduler scheduler;
    scheduler.schedule("a.wav", 96, 24);
    scheduler.schedule("b.wav", 96, 24);

    Expect(scheduler.scheduleFront("visible.wav", 96, 24), "scheduleFront should enqueue a new key");
    Expect(!scheduler.scheduleFront("b.wav", 96, 24), "scheduleFront should dedupe an existing key");
    Expect(scheduler.pendingCount() == 3, "scheduleFront dedupe should keep pending count stable");

    MiniWaveformTextureKey out;
    Expect(scheduler.popNext(out), "front pop should succeed");
    Expect(out == Key("b.wav", 96, 24), "existing front schedule should promote key to queue head");
    Expect(scheduler.popNext(out), "second front pop should succeed");
    Expect(out == Key("visible.wav", 96, 24), "new front schedule should precede older queue items");
    Expect(scheduler.popNext(out), "third front pop should succeed");
    Expect(out == Key("a.wav", 96, 24), "older queue item should remain after front priorities");
}

void TestTrimBackExcessPreservesFrontPriority()
{
    MiniWaveformScheduler scheduler;
    scheduler.schedule("old-a.wav", 96, 24);
    scheduler.schedule("old-b.wav", 96, 24);
    scheduler.schedule("old-c.wav", 96, 24);
    scheduler.scheduleFront("visible-a.wav", 96, 24);
    scheduler.scheduleFront("visible-b.wav", 96, 24);

    scheduler.trimBackExcess(3);

    Expect(scheduler.pendingCount() == 3, "trimBackExcess should reduce pending count to limit");
    Expect(scheduler.contains(Key("visible-a.wav", 96, 24)), "trimBackExcess should preserve high-priority front key");
    Expect(scheduler.contains(Key("visible-b.wav", 96, 24)), "trimBackExcess should preserve newest high-priority front key");
    Expect(scheduler.contains(Key("old-a.wav", 96, 24)), "trimBackExcess should keep oldest remaining item near front");
    Expect(!scheduler.contains(Key("old-b.wav", 96, 24)), "trimBackExcess should drop older tail item");
    Expect(!scheduler.contains(Key("old-c.wav", 96, 24)), "trimBackExcess should drop queue tail item");
}

void TestScheduleFrontCoalescesByPath()
{
    MiniWaveformScheduler scheduler;
    scheduler.schedule("same.wav", 96, 20);
    scheduler.schedule("older.wav", 96, 24);
    scheduler.schedule("same.wav", 128, 32);

    Expect(!scheduler.scheduleFrontCoalescedByPath("same.wav", 96, 20), "coalesced exact duplicate should not add a key");
    Expect(scheduler.pendingCount() == 3, "coalesced exact duplicate should not change pending count");

    MiniWaveformTextureKey out;
    Expect(scheduler.popNext(out), "coalesced exact duplicate pop should succeed");
    Expect(out == Key("same.wav", 96, 20), "coalesced exact duplicate should promote key to front");
    Expect(!scheduler.contains(Key("same.wav", 96, 20)), "promoted exact duplicate should be removed after pop");

    Expect(scheduler.scheduleFrontCoalescedByPath("same.wav", 256, 40), "coalesced size update should replace same-path entries");
    Expect(scheduler.pendingCount() == 2, "coalesced size update should remove remaining old same-path variants");
    Expect(scheduler.contains(Key("same.wav", 256, 40)), "coalesced size update should keep latest key");
    Expect(!scheduler.contains(Key("same.wav", 96, 20)), "coalesced size update should drop old size variant");
    Expect(!scheduler.contains(Key("same.wav", 128, 32)), "coalesced size update should drop second old size variant");

    Expect(scheduler.popNext(out), "coalesced front pop should succeed");
    Expect(out == Key("same.wav", 256, 40), "coalesced latest key should move to front");
    Expect(!scheduler.containsPath("same.wav"), "coalesced path count should clear after pop");
    Expect(scheduler.containsPath("older.wav"), "unrelated path should remain pending");
}

void TestScheduleBackIfPathAbsent()
{
    MiniWaveformScheduler scheduler;
    scheduler.schedule("retry.wav", 96, 20);

    Expect(!scheduler.scheduleBackIfPathAbsent("retry.wav", 256, 40), "retry should no-op when same path is pending");
    Expect(scheduler.pendingCount() == 1, "retry no-op should keep pending count stable");
    Expect(!scheduler.contains(Key("retry.wav", 256, 40)), "retry no-op should not add another size variant");

    Expect(scheduler.scheduleBackIfPathAbsent("new.wav", 128, 24), "retry should enqueue absent path");
    Expect(scheduler.pendingCount() == 2, "retry should increase pending count for absent path");

    MiniWaveformTextureKey out;
    Expect(scheduler.popNext(out), "first retry pop should succeed");
    Expect(out == Key("retry.wav", 96, 20), "existing retry key should keep FIFO order");
    Expect(scheduler.popNext(out), "second retry pop should succeed");
    Expect(out == Key("new.wav", 128, 24), "absent retry key should be queued at back");
}

void TestVisibleTrimPriority()
{
    MiniWaveformScheduler scheduler;
    scheduler.schedule("a.wav", 96, 24);
    scheduler.schedule("b.wav", 96, 24);
    scheduler.schedule("c.wav", 96, 24);
    scheduler.schedule("d.wav", 96, 24);
    scheduler.setVisibleKeys({Key("b.wav", 96, 24), Key("d.wav", 96, 24)});

    Expect(scheduler.isVisible(Key("b.wav", 96, 24)), "visible key should be queryable");
    Expect(!scheduler.isVisible(Key("a.wav", 96, 24)), "non-visible key should be queryable");

    scheduler.trimExcess(2);

    Expect(scheduler.pendingCount() == 2, "trim should reduce pending count to limit");
    Expect(!scheduler.contains(Key("a.wav", 96, 24)), "trim should drop non-visible head key");
    Expect(!scheduler.contains(Key("c.wav", 96, 24)), "trim should drop non-visible key before visible key");
    Expect(scheduler.contains(Key("b.wav", 96, 24)), "trim should preserve visible key");
    Expect(scheduler.contains(Key("d.wav", 96, 24)), "trim should preserve visible key");
}

void TestTrimDropsVisibleWhenStillOverLimit()
{
    MiniWaveformScheduler scheduler;
    scheduler.schedule("a.wav", 96, 24);
    scheduler.schedule("b.wav", 96, 24);
    scheduler.schedule("c.wav", 96, 24);
    scheduler.setVisibleKeys({Key("a.wav", 96, 24), Key("b.wav", 96, 24), Key("c.wav", 96, 24)});

    scheduler.trimExcess(1);

    Expect(scheduler.pendingCount() == 1, "trim should drop visible keys if still above limit");
    Expect(!scheduler.contains(Key("a.wav", 96, 24)), "second trim phase should drop oldest visible key");
    Expect(!scheduler.contains(Key("b.wav", 96, 24)), "second trim phase should continue from queue head");
    Expect(scheduler.contains(Key("c.wav", 96, 24)), "second trim phase should keep newest remaining key");
}

void TestLargeTrimPreservesVisibleInOneQueuePass()
{
    MiniWaveformScheduler scheduler;
    std::vector<MiniWaveformTextureKey> visible;
    const int itemCount = 2000;

    for (int i = 0; i < itemCount; ++i)
    {
        const std::string path = "file_" + std::to_string(i) + ".wav";
        scheduler.schedule(path, 96, 24);
        if (i == 10 || i == 250 || i == 500 || i == 750 ||
            i == 1000 || i == 1250 || i == 1500 || i == 1750)
        {
            visible.push_back(Key(path, 96, 24));
        }
    }

    scheduler.setVisibleKeys(visible);
    scheduler.trimExcess(12);

    Expect(scheduler.pendingCount() == 12, "large trim should reduce pending count to limit");
    for (const MiniWaveformTextureKey& key : visible)
    {
        Expect(scheduler.contains(key), "large trim should preserve representative visible keys");
    }
    Expect(!scheduler.contains(Key("file_0.wav", 96, 24)), "large trim should drop early non-visible keys");
    Expect(!scheduler.contains(Key("file_1800.wav", 96, 24)), "large trim should drop non-visible keys in queue order");
    Expect(scheduler.contains(Key("file_1999.wav", 96, 24)), "large trim may keep newest non-visible keys once limit is met");
}

void TestRemoveAndClear()
{
    MiniWaveformScheduler scheduler;
    scheduler.schedule("a.wav", 96, 24);
    scheduler.schedule("b.wav", 96, 24);
    scheduler.setVisibleKeys({Key("a.wav", 96, 24)});

    scheduler.remove(Key("a.wav", 96, 24));
    Expect(scheduler.pendingCount() == 1, "remove should update pending count");
    Expect(!scheduler.contains(Key("a.wav", 96, 24)), "remove should erase pending key");
    Expect(!scheduler.containsPath("a.wav"), "remove should erase pending path count");
    Expect(scheduler.isVisible(Key("a.wav", 96, 24)), "remove should not clear visible state");

    scheduler.clear();
    Expect(scheduler.pendingCount() == 0, "clear should remove all pending keys");
    Expect(!scheduler.contains(Key("b.wav", 96, 24)), "clear should empty pending set");
    Expect(!scheduler.containsPath("b.wav"), "clear should empty pending path counts");
    Expect(!scheduler.isVisible(Key("a.wav", 96, 24)), "clear should remove visible keys");
}

void TestSelectedWaveformRebuildQueueIsFrameBudgeted()
{
    std::vector<SelectedWaveformRebuildItem> items;
    items.reserve(500);
    for (int i = 0; i < 500; ++i) {
        SelectedWaveformRebuildItem item;
        item.id = i + 1;
        item.filePath = "H:/library/file_" + std::to_string(i) + ".wav";
        items.push_back(item);
    }
    items.push_back({});
    items.push_back(items[7]);

    SelectedWaveformRebuildQueue queue;
    queue.start(items);
    Expect(queue.active(), "selected rebuild queue should become active");
    Expect(queue.remaining() == 500, "selected rebuild queue should ignore empty and duplicate paths");

    int processed = 0;
    const size_t firstPump = queue.pump(16, [&](const SelectedWaveformRebuildItem&) {
        ++processed;
    });
    Expect(firstPump == 16, "selected rebuild queue should process one frame budget");
    Expect(processed == 16, "selected rebuild queue should not process all selected files immediately");
    Expect(queue.active(), "selected rebuild queue should remain active after a partial pump");
    Expect(queue.remaining() == 484, "selected rebuild queue should retain unprocessed items");

    size_t safety = 0;
    while (queue.active() && safety++ < 100) {
        queue.pump(64, [&](const SelectedWaveformRebuildItem&) {
            ++processed;
        });
    }
    Expect(!queue.active(), "selected rebuild queue should finish after all chunks");
    Expect(processed == 500, "selected rebuild queue should eventually process every unique item");
    Expect(queue.remaining() == 0, "selected rebuild queue should clear after completion");
}

} // namespace

int main()
{
    TestDedupe();
    TestFifoPop();
    TestScheduleFrontPriority();
    TestTrimBackExcessPreservesFrontPriority();
    TestScheduleFrontCoalescesByPath();
    TestScheduleBackIfPathAbsent();
    TestVisibleTrimPriority();
    TestTrimDropsVisibleWhenStillOverLimit();
    TestLargeTrimPreservesVisibleInOneQueuePass();
    TestRemoveAndClear();
    TestSelectedWaveformRebuildQueueIsFrameBudgeted();

    if (g_failed != 0)
    {
        std::cerr << "FAIL: " << g_failed << " MiniWaveformScheduler checks failed\n";
        return 1;
    }

    std::cout << "PASS: MiniWaveformScheduler\n";
    return 0;
}
