
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>

int main()
{
    using namespace ftxui;

    // The input field stores whatever the user types.
    std::string message;

    InputOption input_option;
    input_option.multiline = false;

    Component input = Input(&message, "Type a message... ", input_option);

    // Build the UI while keeping the input component interactive.
    auto component = Renderer(input, [&] {
        return vbox({
            text(" CHATAPP ") | bold | center,
            text(" Connected as: atman ") | dim,
            separator(),

            vbox({
                text("rahul: Hey!"),
                text("atman: Hello!"),
                text("rahul: How are you?"),
            }) | flex | border,

            hbox({
                text(" Message: "),
                input->Render() | flex,
            }) | border,

            text(" Press Ctrl+C to exit ") | dim,
        }) | border;
    });

    // Start the interactive terminal screen.
    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(component);

    return 0;
}
