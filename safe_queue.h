#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class SafeQueue {
    std::queue<T> q;
    std::mutex m;
    std::condition_variable cv;
    bool closed_ = false;
public:
    void push(const T& value) {
        std::lock_guard<std::mutex> lock(m);
        q.push(value);
        cv.notify_one();
    }
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&]{ return closed_ || !q.empty(); });
        if (q.empty()) return false; // 队列空且已关闭
        value = q.front();
        q.pop();
        return true;
    }
    bool empty() {
        std::lock_guard<std::mutex> lock(m);
        return q.empty();
    }
    void close() {
        std::lock_guard<std::mutex> lock(m);
        closed_ = true;
        cv.notify_all();
    }
}; 