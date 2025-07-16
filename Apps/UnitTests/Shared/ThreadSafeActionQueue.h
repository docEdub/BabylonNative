#pragma once

#include <queue>
#include <mutex>

class thread_safe_action_queue
{
public:
    void queueAction(const std::function<void()> action)
    {
        std::lock_guard<std::mutex> lock(mtx);
        data.push(action);
    }

    void performQueuedActions()
    {
        std::queue<std::function<void()>> currentData;
        {
            std::lock_guard<std::mutex> lock(mtx);
            currentData = std::move(data);
        }

        while (!currentData.empty())
        {
            std::function<void()> action = currentData.front();
            currentData.pop();
            action();
        }
    }

private:
    std::queue<std::function<void()>> data;
    mutable std::mutex mtx;
};
