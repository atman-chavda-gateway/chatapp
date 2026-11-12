#include <iostream>
#include <asio.hpp>

int main()
{
    asio::io_context io;

    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), 8080);

    asio::ip::tcp::acceptor acceptor(io, endpoint);

    asio::ip::tcp::socket client_socket(io);

    acceptor.accept(client_socket);

    std::cout << "ASIO initialized" << std::endl;
    return 0;
}