
#include "network_client.hpp"

#include <array>
#include <utility>

NetworkClient::NetworkClient(NotifyCallback on_event)
    : on_event_(std::move(on_event))
{
}

NetworkClient::~NetworkClient()
{
    // Stop the receiver loop and unblock any pending socket read.
    connected_.store(false, std::memory_order_release);

    asio::error_code ignored;

    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);

    // Join before the callback, queue and mutex are destroyed.
    if (receiver_.joinable())
        receiver_.join();
}

bool NetworkClient::connect(const std::string& username)
{
    if (socket_.is_open() || receiver_.joinable())
        return false;

    asio::error_code error;

    const auto address = asio::ip::make_address("127.0.0.1", error);

    if (error)
    {
        push_event({
            "Failed to resolve server address: " + error.message(),
            true
        });

        return false;
    }

    const asio::ip::tcp::endpoint server(address, 8080);

    socket_.connect(server, error);

    if (error)
    {
        push_event({
            "Failed to connect: " + error.message(),
            true
        });

        return false;
    }

    // The first frame sent to the server is the username.
    std::string username_frame = username;
    username_frame.push_back('\n');

    asio::write(socket_, asio::buffer(username_frame), error);

    if (error)
    {
        push_event({
            "Failed to send username: " + error.message(),
            true
        });

        socket_.close(error);
        return false;
    }

    connected_.store(true, std::memory_order_release);

    receiver_ = std::jthread([this]
    {
        receive_messages();
    });

    return true;
}

bool NetworkClient::send_message(std::string_view message)
{
    if (!connected())
        return false;

    std::string outgoing = encode_message(message);
    outgoing.push_back('\n');

    asio::error_code error;

    asio::write(socket_, asio::buffer(outgoing), error);

    if (error)
    {
        connected_.store(false, std::memory_order_release);

        push_event({
            "Send failed: " + error.message(),
            true
        });

        return false;
    }

    // The server broadcasts messages back to their sender too.
    // Therefore, we don't add a local copy here.

    return true;
}

bool NetworkClient::try_pop_event(UiEvent& event)
{
    std::lock_guard lock(incoming_events_mutex_);

    if (incoming_events_.empty())
        return false;

    event = std::move(incoming_events_.front());
    incoming_events_.pop();

    return true;
}

bool NetworkClient::connected() const noexcept
{
    return connected_.load(std::memory_order_acquire);
}

void NetworkClient::receive_messages()
{
    std::array<char, 1024> buffer;
    std::string pending_data;

    while (connected())
    {
        asio::error_code error;

        const std::size_t bytes_received = socket_.read_some(
            asio::buffer(buffer),
            error
        );

        // Process received bytes before handling a possible read error.
        if (bytes_received > 0)
        {
            pending_data.append(buffer.data(), bytes_received);

            std::size_t newline_pos;

            while ((newline_pos = pending_data.find('\n'))
                   != std::string::npos)
            {
                std::string frame = pending_data.substr(0, newline_pos);

                pending_data.erase(0, newline_pos + 1);

                push_event({
                    decode_message(frame),
                    false
                });
            }
        }

        if (error)
        {
            // If shutdown was initiated by the destructor, connected_
            // will already be false. Don't report that as a disconnect.
            if (connected_.exchange(false, std::memory_order_acq_rel))
            {
                const std::string status =
                    error == asio::error::eof
                        ? "Server disconnected"
                        : "Connection error: " + error.message();

                push_event({
                    status,
                    true
                });
            }

            break;
        }
    }
}

void NetworkClient::push_event(UiEvent event)
{
    {
        std::lock_guard lock(incoming_events_mutex_);

        incoming_events_.push(std::move(event));
    }

    // Notify only after releasing the queue mutex.
    notify_ui();
}

void NetworkClient::notify_ui()
{
    std::lock_guard lock(notify_mutex_);

    if (on_event_)
        on_event_();
}

std::string NetworkClient::encode_message(std::string_view message)
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

std::string NetworkClient::decode_message(std::string_view message)
{
    std::string decoded;
    decoded.reserve(message.size());

    for (std::size_t i = 0; i < message.size(); ++i)
    {
        if (message[i] == '\\' && i + 1 < message.size())
        {
            const char next = message[i + 1];

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
