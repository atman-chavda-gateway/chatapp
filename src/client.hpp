#pragma once

#include <asio.hpp>
#include <array>
#include <functional>
#include <mutex>
#include <print>
#include <string_view>
#include <utility>
#include <string>

class Client
{
private:
    asio::ip::tcp::socket socket_;
    std::move_only_function<void(Client*)> on_disconnect_; // C++23 upgrade
    std::mutex write_mutex_;
    std::move_only_function<void(std::string_view, std::string_view)> on_message_;
    std::string username_;

public:
    Client(
        asio::ip::tcp::socket socket,
        std::move_only_function<void(Client*)> on_disconnect,
        std::move_only_function<void(std::string_view, std::string_view)> on_message
        ) : 
        socket_(std::move(socket)),
        on_disconnect_(std::move(on_disconnect)),
        on_message_(std::move(on_message))
    {
    }

    const std::string& username() const
    {
        return username_;
    }

    void start_reading()
    {
        bool username_received = false;
        std::array<char, 1024> buffer; // Safer modern C++ array
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
                    username_received = true;
                    std::println("Username set to: {}", username_);
                    continue;
                }

                if (on_message_)
                    on_message_(username_, message);
            }
        }

        if (on_disconnect_)
            on_disconnect_(this);
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
