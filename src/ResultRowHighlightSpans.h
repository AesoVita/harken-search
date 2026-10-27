#pragma once

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

inline void AppendResultRowHighlightSpan(std::vector<std::pair<size_t, size_t>>& spans, size_t start, size_t end) {
    if (end <= start) return;

    auto it = std::lower_bound(
        spans.begin(), spans.end(), start,
        [](const std::pair<size_t, size_t>& span, size_t value) {
            return span.first < value;
        });
    it = spans.emplace(it, start, end);

    if (it != spans.begin()) {
        auto previous = std::prev(it);
        if (previous->second >= it->first) {
            previous->second = (std::max)(previous->second, it->second);
            it = spans.erase(it);
            it = previous;
        }
    }
    while (std::next(it) != spans.end() && std::next(it)->first <= it->second) {
        it->second = (std::max)(it->second, std::next(it)->second);
        spans.erase(std::next(it));
    }
}
