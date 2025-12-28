//
// Created by black on 25. 12. 27..
//
// render/RenderQueue.hpp
#pragma once
#include <vector>
#include <algorithm>
#include "RenderCommand.hpp"

namespace rts::core::render {

    class RenderQueue {
    public:
        using Container = std::vector<RenderCommand>;

        void clear() {
            m_commands.clear();
        }

        void reserve(size_t count) {
            m_commands.reserve(count);
        }

        void push(RenderCommand&& cmd) {
            m_commands.emplace_back(std::move(cmd));
        }

        template<typename T>
        void emplace(RenderLayer layer, int z, T&& data) {
            m_commands.push_back(RenderCommand{
                layer,
                z,
                std::forward<T>(data)
            });
        }

        void sort() {
            std::sort(
                m_commands.begin(),
                m_commands.end(),
                [](const RenderCommand& a, const RenderCommand& b) {
                    if (a.layer != b.layer)
                        return a.layer < b.layer;
                    return a.zOrder < b.zOrder;
                }
            );
        }

        const Container& commands() const {
            return m_commands;
        }

    private:
        Container m_commands;
    };

} // namespace rts::render
