#pragma once

#include <asio.hpp>
#include <array>
#include <functional>
#include <mutex>
#include <print>
#include <string_view>
#include <utility>

class Client
{
private:
    asio::ip::tcp::socket socket_;
    std::move_only_function<void(Client*)> on_disconnect_; // C++23 upgrade
    std::mutex write_mutex_;
    std::move_only_function<void(std::string_view)> on_message_;

public:
    Client(
        asio::ip::tcp::socket socket,
        std::move_only_function<void(Client*)> on_disconnect,
        std::move_only_function<void(std::string_view)> on_message
        ) : 
        socket_(std::move(socket)),
        on_disconnect_(std::move(on_disconnect)),
        on_message_(std::move(on_message))
    {
    }

    void start_reading()
    {
        std::array<char, 1024> buffer; // Safer modern C++ array

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

            // Zero-copy view of the received text
            std::string_view message(buffer.data(), bytes);
            if(on_message_)
                on_message_(message);
        }

        if (on_disconnect_)
            on_disconnect_(this);
    }

    void send_message(std::string_view message)
    {
        std::lock_guard lock(write_mutex_);
        asio::write(socket_, asio::buffer(message));
    }
};
