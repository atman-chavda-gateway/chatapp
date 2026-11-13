#include <asio.hpp>
#include <print>
#include <string>
#include <array>
#include <string_view>

int main()
{
    std::array<char, 1024> buffer;

    asio::io_context io;

    asio::ip::tcp::socket socket(io);

    asio::ip::tcp::endpoint server(asio::ip::make_address("127.0.0.1"), 8080);

    socket.connect(server);

    std::string username = "atman";
    std::string message = username + "\n";

    asio::write(socket, asio::buffer(message));

    std::size_t bytes_received = socket.read_some(
        asio::buffer(buffer)
    );

    std::println("Server: {}", std::string_view(buffer.data(), bytes_received));
    return 0;
}