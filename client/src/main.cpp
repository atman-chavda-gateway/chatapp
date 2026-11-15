
#include "network_client.hpp"
#include "ui.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <print>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::println("Usage: chatapp-client <username>");
        return 1;
    }

    const std::string username = argv[1];

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    NetworkClient network([&screen]
    {
        screen.PostEvent(ftxui::Event::Custom);
    });

    if (!network.connect(username))
    {
        std::println("Failed to connect to server.");
        return 1;
    }

    run_ui(username, network, screen);

    return 0;
}
