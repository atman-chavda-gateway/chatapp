
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "ui.hpp"
#include "network_client.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    // Windows-specific: check whether Shift is physically pressed.
    bool IsShiftPressed()
    {
#ifdef _WIN32
        return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#else
        return false;
#endif
    }
}

void run_ui(
    std::string_view username,
    NetworkClient& network,
    ftxui::ScreenInteractive& screen
)
{
    using namespace ftxui;

    // -------------------- UI state --------------------

    std::string message;
    std::string network_status = "Connected";
    std::vector<std::string> messages;

    int cursor_position = 0;
    Box input_box;

    // -------------------- Input component --------------------

    InputOption input_option;
    input_option.multiline = true;
    input_option.cursor_position = &cursor_position;

    Component input = Input(
        &message,
        "Type a message... ",
        input_option
    );

    // -------------------- UI component --------------------

    Component component = Renderer(input, [&]
    {
        Elements chat_messages;

        for (const auto& msg : messages)
            chat_messages.push_back(paragraph(msg));

        return vbox({
            text(" CHATAPP ") | bold | center,

            text(
                " Connected as: " +
                std::string(username) +
                " | " +
                network_status
            ) | dim,

            separator(),

            vbox(std::move(chat_messages)) | flex | border,

            hbox({
                text(" Message: "),
                input->Render() | flex | reflect(input_box),
            }) | border,

            text(
                " Enter: Send | Shift+Enter: Newline | "
                "Ctrl+W: Delete Word | Ctrl+C: Exit "
            ) | dim,
        }) | border;
    }) | CatchEvent([&](Event event)
    {
        // 1. Process messages and status updates from NetworkClient.
        if (event == Event::Custom)
        {
            UiEvent incoming;

            while (network.try_pop_event(incoming))
            {
                if (incoming.is_status)
                    network_status = std::move(incoming.text);
                else
                    messages.push_back(std::move(incoming.text));
            }

            return true;
        }

        // 2. Ctrl+W: delete the word immediately before the cursor.
        if (event == Event::CtrlW)
        {
            if (message.empty() || cursor_position <= 0)
                return true;

            std::size_t pos = std::min(
                static_cast<std::size_t>(cursor_position),
                message.size()
            );

            const std::size_t start_pos = pos;

            // Skip whitespace before the word.
            while (pos > 0 &&
                   (message[pos - 1] == ' ' ||
                    message[pos - 1] == '\n' ||
                    message[pos - 1] == '\t'))
            {
                --pos;
            }

            // Skip the word itself.
            while (pos > 0 &&
                   message[pos - 1] != ' ' &&
                   message[pos - 1] != '\n' &&
                   message[pos - 1] != '\t')
            {
                --pos;
            }

            const std::size_t chars_to_delete = start_pos - pos;

            for (std::size_t i = 0; i < chars_to_delete; ++i)
                input->OnEvent(Event::Backspace);

            return true;
        }

        // 3. Enter sends; Shift+Enter inserts a newline.
        if (event == Event::Return)
        {
            if (IsShiftPressed())
                return input->OnEvent(Event::Return);

            // Ignore empty or whitespace-only messages.
            if (message.find_first_not_of(" \n\r\t") == std::string::npos)
                return true;

            if (!network.connected())
            {
                network_status = "Disconnected";
                return true;
            }

            if (!network.send_message(message))
            {
                network_status = "Send failed";
                return true;
            }

            // The server broadcasts messages back to their sender.
            // Therefore, we don't add a local copy here.
            message.clear();
            cursor_position = 0;

            return true;
        }

        // 4. Auto-wrap when typing beyond the available input width.
        if (event.is_character())
        {
            const int usable_width = std::max(
                1,
                (input_box.x_max - input_box.x_min) - 1
            );

            const std::size_t pos = std::min(
                static_cast<std::size_t>(cursor_position),
                message.size()
            );

            const std::size_t last_newline = (pos == 0)
                ? std::string::npos
                : message.rfind('\n', pos - 1);

            const std::size_t current_line_len =
                (last_newline == std::string::npos)
                    ? pos
                    : pos - last_newline - 1;

            if (static_cast<int>(current_line_len) >= usable_width)
                input->OnEvent(Event::Return);
        }

        return false;
    });

    // -------------------- Run UI --------------------

    screen.Loop(component);
}
