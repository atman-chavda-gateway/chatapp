
#pragma once

#include <asio.hpp>

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>

struct UiEvent
{
    std::string text;
    bool is_status = false;
};

class NetworkClient
{
public:
    using NotifyCallback = std::move_only_function<void()>;

    explicit NetworkClient(NotifyCallback on_event);
    ~NetworkClient();

    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    NetworkClient(NetworkClient&&) = delete;
    NetworkClient& operator=(NetworkClient&&) = delete;

    [[nodiscard]] bool connect(const std::string& username);
    [[nodiscard]] bool send_message(std::string_view message);

    [[nodiscard]] bool try_pop_event(UiEvent& event);
    [[nodiscard]] bool connected() const noexcept;

private:
    void receive_messages();
    void push_event(UiEvent event);
    void notify_ui();

    [[nodiscard]] static std::string encode_message(std::string_view message);
    [[nodiscard]] static std::string decode_message(std::string_view message);

    asio::io_context io_;
    asio::ip::tcp::socket socket_{io_};

    std::atomic<bool> connected_{false};
    std::jthread receiver_;

    std::queue<UiEvent> incoming_events_;
    std::mutex incoming_events_mutex_;
    std::mutex notify_mutex_;

    NotifyCallback on_event_;
};
