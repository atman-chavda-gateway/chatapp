#pragma once

#include <asio.hpp>
#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <utility>

class Client
{
public:
    using ConnectCallback = std::move_only_function<bool(std::shared_ptr<Client>)>;
    using MessageCallback = std::move_only_function<void(std::string_view, std::string_view)>;
    using DisconnectCallback = std::move_only_function<void(std::shared_ptr<Client>)>;

private:
    asio::ip::tcp::socket socket_;
    ConnectCallback on_connect_;
    MessageCallback on_message_;
    DisconnectCallback on_disconnect_;

    std::mutex write_mutex_;
    std::string username_;

public:
    Client(
        asio::ip::tcp::socket socket,
        ConnectCallback on_connect,
        MessageCallback on_message,
        DisconnectCallback on_disconnect
    ) :
        socket_(std::move(socket)),
        on_connect_(std::move(on_connect)),
        on_message_(std::move(on_message)),
        on_disconnect_(std::move(on_disconnect))
    {
    }

    const std::string& username() const
    {
        return username_;
    }

    void start_reading(std::shared_ptr<Client> self)
    {
        bool username_received = false;
        std::array<char, 1024> buffer;
        std::string pending_data;

        while (true)
        {
            asio::error_code error;
            std::size_t bytes = socket_.read_some(asio::buffer(buffer), error);

            if (error)
            {
                if (error == asio::error::eof)
                    std::println("Client disconnected");
                else
                    std::println("Read error: {}", error.message());

                break;
            }

            pending_data.append(buffer.data(), bytes);

            std::size_t newline_pos;

            while ((newline_pos = pending_data.find('\n')) != std::string::npos)
            {
                std::string message = pending_data.substr(0, newline_pos);

                pending_data.erase(0, newline_pos + 1);

                if (!username_received)
                {
                    username_ = message;
                    
                    if (on_connect_ && !on_connect_(self))
                    {
                        break;
                    }
                    username_received = true;
                    std::println("Username set to: {}", username_);
                    
                    continue;
                }

                if (on_message_)
                    on_message_(username_, message);
            }
        }

        if (on_disconnect_)
            on_disconnect_(std::move(self));
    }

    void send_message(std::string_view message)
    {
        std::lock_guard lock(write_mutex_);

        std::string framed_message(message);
        framed_message.push_back('\n');

        asio::error_code error;
        asio::write(socket_, asio::buffer(framed_message), error);

        if (error)
            std::println("Send error: {}", error.message());
    }
};
