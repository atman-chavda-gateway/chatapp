
#pragma once

#include <ftxui/component/screen_interactive.hpp>

#include <string_view>

class NetworkClient;

void run_ui(
    std::string_view username,
    NetworkClient& network,
    ftxui::ScreenInteractive& screen
);
