// src/main.cpp
#include "core/GameApp.hpp"
#include "core/DIContainer.hpp"

int main()
{
    rts::core::DIContainer di;
    rts::core::GameApp app(di);
    app.run();
    return 0;
}
