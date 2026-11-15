#include <asio.hpp>
#include <print>
#include <string>
#include <thread>
#include <array>
#include <string_view>
#include <iostream>

int main(int argc, char* argv[])
{

    if (argc != 2)
    {
        std::println("Usage: chatapp-client <username>");
        return 1;
    }

    asio::io_context io;
    std::string username = argv[1];
    std::string message = username + "\n";

    asio::ip::tcp::socket socket(io);

    asio::ip::tcp::endpoint server(asio::ip::make_address("127.0.0.1"), 8080);

    socket.connect(server);

    asio::write(socket, asio::buffer(message));

    std::jthread receiver([&socket]()
    {
        std::array<char, 1024> buffer;

        while (true)
        {
            asio::error_code error;

            std::size_t bytes_received = socket.read_some(
                asio::buffer(buffer),
                error
            );

            if (error)
            {
                break;
            }

            std::println(
                "{}",
                std::string_view(buffer.data(), bytes_received)
            );
        }
    });

    while (std::getline(std::cin, message))
    {
        if (message == "/quit")
            break;

        std::string outgoing = message + "\n";

        asio::write(socket, asio::buffer(outgoing));
    }   

    asio::error_code error;
    socket.shutdown(asio::ip::tcp::socket::shutdown_both, error);
    socket.close(error);

    receiver.join();

    return 0;
}