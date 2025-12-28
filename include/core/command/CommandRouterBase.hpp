#pragma once

#include <unordered_map>
#include <typeindex>
#include <vector>
#include <functional>

#include "LogicCommand.hpp"
#include "UICommand.hpp"

namespace rts::command {

    template<typename BaseCommand>
    class CommandRouterBase {
    public:
        CommandRouterBase() = default;

        // 핸들러 등록
        template<typename T>
        void on(std::function<void(const T&)> handler) {
            static_assert(std::is_base_of_v<BaseCommand, T>,
                          "T must derive from BaseCommand");

            auto& list = m_handlers[std::type_index(typeid(T))];

            list.emplace_back(
                [fn = std::move(handler)](const BaseCommand& cmd) {
                    fn(static_cast<const T&>(cmd));
                }
            );
        }

        // dispatch
        void dispatch(const BaseCommand& cmd) const {
            auto it = m_handlers.find(std::type_index(typeid(cmd)));
            if (it == m_handlers.end())
                return;

            for (const auto& handler : it->second) {
                handler(cmd);
            }
        }

        void clear() {
            m_handlers.clear();
        }

    private:
        using Handler = std::function<void(const BaseCommand&)>;

        std::unordered_map<
            std::type_index,
            std::vector<Handler>
        > m_handlers;
    };

    // =========================================================
    // Concrete Routers
    // =========================================================
    class UICommandRouter final
        : public CommandRouterBase<UICommand> {};

    class LogicCommandRouter final
        : public CommandRouterBase<LogicCommand> {};

} // namespace rts::command
