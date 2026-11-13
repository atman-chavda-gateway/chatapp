#include <asio.hpp>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <print>

#include "client.hpp"

int main()
{
    asio::io_context io;

    asio::ip::tcp::acceptor acceptor(
        io,
        {asio::ip::tcp::v4(), 8080}
    );

    std::vector<std::shared_ptr<Client>> clients;
    std::mutex clients_mutex;
    std::mutex broadcast_mutex;

    std::println("Server listening on port 8080");

    while (true)
    {
        asio::ip::tcp::socket socket = acceptor.accept();

        auto client = std::make_shared<Client>(
            std::move(socket),
            [&clients, &clients_mutex](Client* disconnected)
            {
                std::lock_guard lock(clients_mutex);

                std::erase_if(clients, [disconnected](const auto& c)
                {
                    return c.get() == disconnected;
                });

                std::println(
                    "Client removed. Remaining: {}",
                    clients.size()
                );
            },

            [&clients, &clients_mutex, &broadcast_mutex](std::string_view message)
            {
                std::lock_guard broadcast_lock(broadcast_mutex);
                
                std::vector<std::shared_ptr<Client>> recipients;

                {
                    std::lock_guard lock(clients_mutex);
                    recipients = clients; 
                }
                
                for(const auto& recipient : recipients)
                {
                    recipient->send_message(message);
                }
            }
        );

        {
            std::lock_guard lock(clients_mutex);

            clients.push_back(client);

            std::println(
                "Client connected. Total: {}",
                clients.size()
            );
        }

        std::jthread([client]
        {
            client->start_reading();
        }).detach();
    }
}
