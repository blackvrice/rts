//
// Created by black on 25. 12. 21..
//

#pragma once

#include <unordered_map>
#include <typeindex>
#include <vector>
#include <functional>

#include "Command.hpp"

namespace rts::command {

    class CommandRouter {
    public:
        CommandRouter() = default;

        // 핸들러 등록 (Manager::start()에서 호출)
        template<typename T>
        void on(std::function<void(const T&)> handler) {
            static_assert(std::is_base_of_v<Command, T>,
                          "T must derive from Command");

            auto& list = m_handlers[std::type_index(typeid(T))];

            // type-erased wrapper
            list.emplace_back(
                [fn = std::move(handler)](const Command& cmd) {
                    fn(static_cast<const T&>(cmd));
                }
            );
        }

        // LogicThread에서 pop된 Command를 전달
        void dispatch(const Command& cmd) const {
            auto it = m_handlers.find(std::type_index(typeid(cmd)));
            if (it == m_handlers.end())
                return;

            for (const auto& handler : it->second) {
                handler(cmd);
            }
        }

        // Scene 전환 시 정리용 (선택)
        void clear() {
            m_handlers.clear();
        }

    private:
        using Handler = std::function<void(const Command&)>;

        std::unordered_map<
            std::type_index,
            std::vector<Handler>
        > m_handlers;
    };

} // namespace rts::core
