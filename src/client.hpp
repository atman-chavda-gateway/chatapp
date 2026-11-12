#pragma once

#include <asio.hpp>
#include <array>
#include <functional>
#include <print>
#include <string_view>
#include <utility>

class Client
{
private:
    asio::ip::tcp::socket socket_;
    std::move_only_function<void(Client*)> on_disconnect_; // C++23 upgrade

public:
    Client(
        asio::ip::tcp::socket socket,
        std::move_only_function<void(Client*)> on_disconnect
    )
        : socket_(std::move(socket)),
          on_disconnect_(std::move(on_disconnect))
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
            std::println("Received: {}", message);
        }

        if (on_disconnect_)
            on_disconnect_(this);
    }
};
