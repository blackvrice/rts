//
// Created by black on 25. 12. 25..
//

#pragma once
#include <algorithm>
#include <functional>
#include <mutex>
namespace rts::core{
    template<typename... Args>
    class Signal {
    public:
        using Slot = std::function<void(Args...)>;

        size_t operator+=(Slot slot) {
            std::lock_guard lock(mutex);
            slots.emplace_back(++nextId, std::move(slot));
            return nextId;
        }

        void operator-=(size_t id) {
            std::lock_guard lock(mutex);
            slots.erase(
                std::remove_if(slots.begin(), slots.end(),
                               [&](auto &p) { return p.first == id; }),
                slots.end());
        }

        void operator()(Args... args) {
            std::vector<Slot> copy;
            {
                std::lock_guard lock(mutex);
                for (auto &[id, s]: slots)
                    copy.push_back(s);
            }
            for (auto &s: copy)
                s(args...);
        }

    private:
        std::mutex mutex;
        size_t nextId = 0;
        std::vector<std::pair<size_t, Slot> > slots;
    };
}
