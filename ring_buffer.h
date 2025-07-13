#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>

// 线程安全的环形缓冲区模板
// 用于音视频帧的生产者-消费者模型
// 支持多线程 push/pop，满时写入阻塞，空时读取阻塞

template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity), head_(0), tail_(0), size_(0) {}

    // 写入数据，满时阻塞
    void push(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [&] { return size_ < capacity_; });
        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
        not_empty_.notify_one();
    }

    // 读取数据，空时阻塞
    void pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [&] { return size_ > 0; });
        item = buffer_[head_];
        head_ = (head_ + 1) % capacity_;
        --size_;
        not_full_.notify_one();
    }

    // 当前元素个数
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    // 缓冲区是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == 0;
    }

    // 缓冲区是否已满
    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == capacity_;
    }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t head_;
    size_t tail_;
    size_t size_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};
