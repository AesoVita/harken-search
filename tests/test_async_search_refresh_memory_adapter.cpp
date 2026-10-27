#include "AsyncSearchRefreshMemoryAdapter.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

int main() {
    std::uint64_t requestedEviction = 0;
    std::vector<std::string> released;
    std::size_t fallbackSize = 4;
    std::size_t reportedSize = 0;
    std::size_t reportedLimit = 0;

    aura::search::AsyncSearchRefreshMemoryAdapter budgetAdapter(
        [] {
            return aura::search::AsyncSearchRefreshMemorySnapshot{
                true, 25, 10, 2};
        },
        [&](std::uint64_t overBudget) {
            requestedEviction = overBudget;
            return std::vector<std::string>{"first", "second"};
        },
        [&](const std::string& key) { released.push_back(key); },
        [&] { return fallbackSize; },
        [&] { --fallbackSize; },
        [&](std::size_t size, std::size_t limit) {
            reportedSize = size;
            reportedLimit = limit;
        });
    budgetAdapter.apply();
    assert(requestedEviction == 15);
    assert((released == std::vector<std::string>{"first", "second"}));
    assert(fallbackSize == 4);
    assert(reportedSize == 0 && reportedLimit == 0);

    aura::search::AsyncSearchRefreshMemoryAdapter fallbackAdapter(
        [] {
            return aura::search::AsyncSearchRefreshMemorySnapshot{
                false, 0, 0, 2};
        },
        [](std::uint64_t) { return std::vector<std::string>{}; },
        [](const std::string&) {},
        [&] { return fallbackSize; },
        [&] { --fallbackSize; },
        [&](std::size_t size, std::size_t limit) {
            reportedSize = size;
            reportedLimit = limit;
        });
    fallbackAdapter.apply();
    assert(fallbackSize == 2);
    assert(reportedSize == 4 && reportedLimit == 2);
    return 0;
}
