#ifndef ROV_FAKE_TRANSPORT_HPP
#define ROV_FAKE_TRANSPORT_HPP

#include "internal/transport.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

class FakeTransport final : public rov::internal::ITransport {
public:
    using CommandHandler = std::function<void(const std::string&,
                                               FakeTransport&)>;

    rov::internal::OpenResult open() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (failOpen_) {
            return {rov::internal::IoStatus::Error, 5, "fake open failure"};
        }
        open_ = true;
        disconnected_ = false;
        condition_.notify_all();
        return {rov::internal::IoStatus::Ok, 0, {}};
    }

    void close() noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        open_ = false;
        condition_.notify_all();
    }

    bool isOpen() const noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return open_ && !disconnected_;
    }

    rov::internal::ReadResult readSome(
        std::chrono::milliseconds timeout) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait_for(lock, timeout, [this] {
            return !readChunks_.empty() || disconnected_ || !open_;
        });
        if (!readChunks_.empty()) {
            std::string data = std::move(readChunks_.front());
            readChunks_.pop_front();
            return {rov::internal::IoStatus::Ok, std::move(data), 0, {}};
        }
        if (disconnected_ || !open_) {
            return {rov::internal::IoStatus::Disconnected, {}, 0,
                    "fake disconnected"};
        }
        return {rov::internal::IoStatus::Timeout, {}, 0, {}};
    }

    void enqueueRead(std::string data)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        readChunks_.push_back(std::move(data));
        condition_.notify_all();
    }

    void disconnect()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        disconnected_ = true;
        condition_.notify_all();
    }

    void setCommandHandler(CommandHandler handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handler_ = std::move(handler);
    }

    void setMaxWriteChunk(std::size_t size)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        maxWriteChunk_ = size;
    }

    void setRetryWrites(unsigned int count)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        retryWrites_ = count;
    }

    void setFailOpen(bool fail)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        failOpen_ = fail;
    }

    void setFailWrites(bool fail)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        failWrites_ = fail;
    }

    bool waitForCommandCount(std::size_t count,
                             std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, timeout, [this, count] {
            return commands_.size() >= count;
        });
    }

    std::vector<std::string> commands() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return commands_;
    }

    std::size_t writeCallCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return writeCallCount_;
    }

protected:
    rov::internal::WriteResult writeSome(const char* data,
                                          std::size_t size) override
    {
        std::vector<std::string> completed;
        CommandHandler handler;
        std::size_t accepted = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++writeCallCount_;
            if (!open_ || disconnected_) {
                return {rov::internal::IoStatus::Disconnected, 0, 0,
                        "fake disconnected"};
            }
            if (failWrites_) {
                return {rov::internal::IoStatus::Error, 0, 5,
                        "fake write failure"};
            }
            if (retryWrites_ > 0U) {
                --retryWrites_;
                return {rov::internal::IoStatus::Retry, 0, 4, {}};
            }
            accepted = std::min(size, maxWriteChunk_);
            writeBuffer_.append(data, accepted);
            for (;;) {
                const std::size_t newline = writeBuffer_.find('\n');
                if (newline == std::string::npos) {
                    break;
                }
                std::string command = writeBuffer_.substr(0, newline);
                writeBuffer_.erase(0, newline + 1U);
                commands_.push_back(command);
                completed.push_back(std::move(command));
            }
            handler = handler_;
            condition_.notify_all();
        }

        if (handler) {
            for (const auto& command : completed) {
                handler(command, *this);
            }
        }
        return {rov::internal::IoStatus::Ok, accepted, 0, {}};
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::string> readChunks_;
    std::vector<std::string> commands_;
    std::string writeBuffer_;
    CommandHandler handler_;
    std::size_t maxWriteChunk_{std::numeric_limits<std::size_t>::max()};
    std::size_t writeCallCount_{0};
    unsigned int retryWrites_{0};
    bool open_{false};
    bool disconnected_{false};
    bool failOpen_{false};
    bool failWrites_{false};
};

inline std::string commandSequence(const std::string& command)
{
    return command.substr(0, 4);
}

inline std::string commandPayload(const std::string& command)
{
    return command.size() > 5 ? command.substr(5) : std::string{};
}

#endif // ROV_FAKE_TRANSPORT_HPP
