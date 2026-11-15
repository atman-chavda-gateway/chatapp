#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

bool IsShiftPressed() {
#ifdef _WIN32
    return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#else
    return false;
#endif
}

int main()
{
    using namespace ftxui;

    std::string message;
    std::string last_key_event = "None";
    std::vector<std::string> messages;
    int cursor_position = 0;

    // Track the rendered bounding box of the input field
    Box input_box;

    InputOption input_option;
    input_option.multiline = true;
    input_option.cursor_position = &cursor_position;

    Component input = Input(&message, "Type a message... ", input_option);

    Component component = Renderer(input, [&] {
        Elements chat_messages;
        for (const auto& msg : messages) {
            chat_messages.push_back(paragraph(msg));
        }

        return vbox({
            text(" CHATAPP ") | bold | center,
            text(" Connected as: atman ") | dim,
            separator(),

            vbox(std::move(chat_messages)) | flex | border,

            hbox({
                text(" Message: "),
                input->Render() | flex | reflect(input_box),
            }) | border,

            text("Last key event: " + last_key_event) | dim,
            text(" Enter: Send | Shift+Enter: Newline | Ctrl+W: Delete Word | Ctrl+C: Exit ") | dim,
        }) | border;
    }) | CatchEvent([&](Event event) {
        last_key_event = event.DebugString();

        // 1. Ctrl+W: Delete the word immediately before the cursor
        if (event == Event::CtrlW) {
            if (message.empty() || cursor_position <= 0) {
                return true;
            }

            size_t pos = std::min(static_cast<size_t>(cursor_position), message.size());
            size_t start_pos = pos;

            // Skip trailing spaces, tabs, or newlines before the word
            while (pos > 0 && (message[pos - 1] == ' ' || message[pos - 1] == '\n' || message[pos - 1] == '\t')) {
                --pos;
            }

            // Skip the word characters until the previous space/newline
            while (pos > 0 && message[pos - 1] != ' ' && message[pos - 1] != '\n' && message[pos - 1] != '\t') {
                --pos;
            }

            // Replay Backspace events so Input updates text and cursor cleanly
            size_t chars_to_delete = start_pos - pos;
            for (size_t i = 0; i < chars_to_delete; ++i) {
                input->OnEvent(Event::Backspace);
            }

            return true;
        }

        // 2. Enter vs Shift+Enter
        if (event == Event::Return) {
            if (IsShiftPressed()) {
                return input->OnEvent(Event::Return);
            }

            if (message.find_first_not_of(" \n\r\t") != std::string::npos) {
                messages.push_back("atman: " + message);
                message.clear();
                cursor_position = 0;
            }
            return true;
        }

        // 3. Auto-wrap when typing a character that exceeds usable width
        if (event.is_character()) {
            int usable_width = std::max(1, (input_box.x_max - input_box.x_min) - 1);

            size_t pos = std::min(static_cast<size_t>(cursor_position), message.size());
            size_t last_newline = (pos == 0) ? std::string::npos : message.rfind('\n', pos - 1);
            size_t current_line_len = (last_newline == std::string::npos)
                ? pos
                : pos - last_newline - 1;

            if (static_cast<int>(current_line_len) >= usable_width) {
                input->OnEvent(Event::Return);
            }
        }

        return false;
    });

    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(component);

    return 0;
}
