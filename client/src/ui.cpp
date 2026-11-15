
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <asio.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <iostream>
#include <mutex>
#include <print>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// Windows-specific: check whether Shift is physically pressed.
bool IsShiftPressed()
{
#ifdef _WIN32
    return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#else
    return false;
#endif
}

// Escape special characters so one message occupies one TCP frame.
std::string EncodeMessage(std::string_view message)
{
    std::string encoded;
    encoded.reserve(message.size());

    for (char ch : message)
    {
        if (ch == '\\')
            encoded += "\\\\";
        else if (ch == '\n')
            encoded += "\\n";
        else
            encoded += ch;
    }

    return encoded;
}

// Restore the original message after receiving a complete frame.
std::string DecodeMessage(std::string_view message)
{
    std::string decoded;
    decoded.reserve(message.size());

    for (std::size_t i = 0; i < message.size(); ++i)
    {
        if (message[i] == '\\' && i + 1 < message.size())
        {
            char next = message[i + 1];

            if (next == '\\')
                decoded += '\\';
            else if (next == 'n')
                decoded += '\n';
            else
            {
                decoded += '\\';
                decoded += next;
            }

            ++i;
        }
        else
        {
            decoded += message[i];
        }
    }

    return decoded;
}

// Items passed from the receiver thread to the UI thread.
struct UiEvent
{
    std::string text;
    bool is_status = false;
};

int main(int argc, char* argv[])
{
    using namespace ftxui;

    if (argc != 2)
    {
        std::println("Usage: chatapp-client <username>");
        return 1;
    }

    const std::string username = argv[1];

    // -------------------- Networking setup --------------------

    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    asio::error_code error;

    const asio::ip::tcp::endpoint server(
        asio::ip::make_address("127.0.0.1"),
        8080
    );

    socket.connect(server, error);

    if (error)
    {
        std::println("Failed to connect: {}", error.message());
        return 1;
    }

    // The first line sent to the server is the username.
    const std::string username_frame = username + '\n';

    asio::write(socket, asio::buffer(username_frame), error);

    if (error)
    {
        std::println("Failed to send username: {}", error.message());
        return 1;
    }

    std::atomic<bool> connected = true;

    // Receiver thread writes here; UI thread reads from here.
    std::queue<UiEvent> incoming_events;
    std::mutex incoming_events_mutex;

    // -------------------- FTXUI setup --------------------

    std::string message;
    std::string network_status = "Connected";
    std::vector<std::string> messages;

    int cursor_position = 0;
    Box input_box;

    InputOption input_option;
    input_option.multiline = true;
    input_option.cursor_position = &cursor_position;

    Component input = Input(&message, "Type a message... ", input_option);

    auto screen = ScreenInteractive::Fullscreen();

    // -------------------- Receiver thread --------------------

    std::jthread receiver([&]()
    {
        std::array<char, 1024> buffer;
        std::string pending_data;

        while (connected)
        {
            asio::error_code read_error;

            std::size_t bytes_received = socket.read_some(
                asio::buffer(buffer),
                read_error
            );

            // Process any bytes received, even if the read also reported
            // an error (for example, EOF after the final bytes).
            if (bytes_received > 0)
            {
                pending_data.append(buffer.data(), bytes_received);

                std::size_t newline_pos;

                while ((newline_pos = pending_data.find('\n')) != std::string::npos)
                {
                    std::string frame = pending_data.substr(0, newline_pos);
                    pending_data.erase(0, newline_pos + 1);

                    UiEvent event;
                    event.text = DecodeMessage(frame);

                    {
                        std::lock_guard lock(incoming_events_mutex);
                        incoming_events.push(std::move(event));
                    }

                    // Ask FTXUI to wake up and process the incoming queue.
                    screen.PostEvent(Event::Custom);
                }
            }

            if (read_error)
            {
                connected = false;

                UiEvent event;
                event.text = read_error == asio::error::eof
                    ? "Server disconnected"
                    : "Connection error: " + read_error.message();
                event.is_status = true;

                {
                    std::lock_guard lock(incoming_events_mutex);
                    incoming_events.push(std::move(event));
                }

                screen.PostEvent(Event::Custom);
                break;
            }
        }
    });

    // -------------------- UI component --------------------

    Component component = Renderer(input, [&]
    {
        Elements chat_messages;

        for (const auto& msg : messages)
            chat_messages.push_back(paragraph(msg));

        return vbox({
            text(" CHATAPP ") | bold | center,
            text(" Connected as: " + username + " | " + network_status) | dim,
            separator(),

            vbox(std::move(chat_messages)) | flex | border,

            hbox({
                text(" Message: "),
                input->Render() | flex | reflect(input_box),
            }) | border,

            text(" Enter: Send | Shift+Enter: Newline | Ctrl+W: Delete Word | Ctrl+C: Exit ") | dim,
        }) | border;
    }) | CatchEvent([&](Event event)
    {
        // 1. Process messages and status updates from the receiver thread.
        if (event == Event::Custom)
        {
            std::queue<UiEvent> pending_events;

            {
                std::lock_guard lock(incoming_events_mutex);
                std::swap(pending_events, incoming_events);
            }

            while (!pending_events.empty())
            {
                UiEvent incoming = std::move(pending_events.front());
                pending_events.pop();

                if (incoming.is_status)
                    network_status = incoming.text;
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

            if (!connected)
            {
                network_status = "Disconnected";
                return true;
            }

            std::string outgoing = EncodeMessage(message);
            outgoing.push_back('\n');

            asio::error_code write_error;

            asio::write(socket, asio::buffer(outgoing), write_error);

            if (write_error)
            {
                connected = false;
                network_status = "Send failed: " + write_error.message();
                return true;
            }

            // The server broadcasts to the sender too, so we don't add
            // a local copy here. The receiver thread will deliver it.
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

    // -------------------- Run and clean up --------------------

    screen.Loop(component);

    // Wake the receiver thread if it is blocked waiting for TCP data.
    connected = false;

    socket.shutdown(asio::ip::tcp::socket::shutdown_both, error);
    socket.close(error);

    if (receiver.joinable())
        receiver.join();

    return 0;
}
