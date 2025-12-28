// src/main.cpp
#include "../include/core/app/GameApp.hpp"
#include "../include/core/di/DIContainer.hpp"

int main()
{
    rts::core::DIContainer di;
    rts::core::GameApp app(di);
    app.run();
    return 0;
}
