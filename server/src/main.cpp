#include <asio.hpp>
#include <memory>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

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

            [&clients, &clients_mutex](std::shared_ptr<Client> connected) -> bool    
            {
                const std::string& name = connected->username();
                
                if(name.empty())
                {
                    std::println("Rejected client: empty username");
                    return false;
                }

                if(name.size() > 20)
                {
                    std::println("Rejected client: username too long");
                    return false;
                }

                {
                    std::lock_guard lock(clients_mutex);

                    for(const auto& client : clients)
                    {
                        if(client->username() == name)
                        {
                            std::println("Rejected client: username already taken");
                            return false;
                        }
                    }
                    clients.push_back(std::move(connected));
                    std::println("Client connected: {}. Total: {}", name, clients.size());
                }                
                return true;
            },

            [&clients, &clients_mutex, &broadcast_mutex](std::string_view username, std::string_view message)
            {
                std::string formatted_message = std::string(username) + ": " + std::string(message);

                std::lock_guard broadcast_lock(broadcast_mutex);

                std::vector<std::shared_ptr<Client>> recipients;

                {
                    std::lock_guard lock(clients_mutex);
                    recipients = clients;
                }

                for (const auto& recipient : recipients)
                {
                    recipient->send_message(formatted_message);
                }
            },

            [&clients, &clients_mutex](std::shared_ptr<Client> disconnected)
            {
                std::lock_guard lock(clients_mutex);

                const auto removed = std::erase(clients, disconnected);

                if(removed > 0)
                {
                    std::println(
                        "Client removed. Remaining: {}",
                        clients.size()
                    );
                }
            }
        );

        std::jthread([client] {
            client->start_reading(client);
        }).detach();
    }
}
