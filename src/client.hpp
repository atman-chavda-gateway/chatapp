#pragma once

#include <asio.hpp>
#include <utility>

class Client
{
    private:
        asio::ip::tcp::socket socket_;

    public:
        Client(asio::ip::tcp::socket socket) : socket_(std::move(socket))
        {}
};
