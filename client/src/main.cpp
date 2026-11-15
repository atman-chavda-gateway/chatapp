#include <asio.hpp>
#include <atomic>
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

    std::string username = argv[1];
    std::string message = username + "\n";
    
    asio::io_context io;
    asio::error_code error;
    asio::ip::tcp::socket socket(io);
    std::atomic<bool> connected = true;

    asio::ip::tcp::endpoint server(asio::ip::make_address("127.0.0.1"), 8080);

    socket.connect(server, error);

    if (error)
    {
        std::println("Failed to connect: {}", error.message());
        return 1;
    }

    asio::write(socket, asio::buffer(message), error);

    if (error)
    {
        std::println("Failed to send username: {}", error.message());
        return 1;
    }
    

    std::println("{} Connected to server", username);

    std::jthread receiver([&socket, &connected]()
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
                connected = false;
                std::println("Server disconnected");
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
        if (!connected)
            break;
    
        if (message == "/quit")
            break;
    
        std::string outgoing = message + "\n";
    
        asio::write(socket, asio::buffer(outgoing), error);
    
        if (error)
        {
            connected = false;
            std::println("Failed to send: {}", error.message());
            break;
        }
    }

    socket.shutdown(asio::ip::tcp::socket::shutdown_both, error);
    socket.close(error);

    receiver.join();

    return 0;
}