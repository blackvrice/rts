#include <functional>
#include <string>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <ui/UIElement.hpp>

namespace rts::ui {
    class Button : public UIElement {
    public:
        Button(sf::Vector2f pos, sf::Vector2f size, std::string text);

        bool handleEvent(const sf::Event& ev) override;

        void render(sf::RenderTarget& t) override {
            t.draw(m_rect);
            t.draw(m_text);
        }

        sf::FloatRect bounds() const override;

        std::function<void()> onClick;

    private:
        sf::RectangleShape m_rect;
        sf::Font m_font;
        sf::Text m_text;
    };

}
