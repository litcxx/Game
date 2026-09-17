#pragma once

#include <concepts>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace ep {
template <typename T>
concept MovableType = std::movable<T>;

template <MovableType T>
class TSQueue {
    template <MovableType U>
    friend void ts_swap(TSQueue<U>& lhs, TSQueue<U>& rhs);

  public:
    TSQueue() = default;
    TSQueue(const TSQueue&) = delete;
    TSQueue& operator=(const TSQueue&) = delete;

    void swap(TSQueue<T>& other);

    std::optional<T> try_pop();
    T wait_and_pop();
    void push(T value);

    bool empty() const noexcept;
    std::size_t size() const noexcept;

  private:
    mutable std::mutex data_mutex_;
    std::condition_variable data_cond_;
    std::queue<T> data_;
};

template <MovableType U>
void ts_swap(TSQueue<U>& lhs, TSQueue<U>& rhs) {
    if (&lhs == &rhs) return;
    std::scoped_lock lock(lhs.data_mutex_, rhs.data_mutex_);
    auto tmp = std::move(lhs.data_);
    lhs.data_ = std::move(rhs.data_);
    rhs.data_ = std::move(tmp);
}

template <MovableType T>
std::optional<T> TSQueue<T>::try_pop() {
    std::lock_guard lock(data_mutex_);
    if (data_.empty()) return std::nullopt;
    auto value = std::make_optional<T>(std::move(data_.front()));
    data_.pop();
    return value;
}

template <MovableType T>
T TSQueue<T>::wait_and_pop() {
    std::unique_lock lock(data_mutex_);
    data_cond_.wait(lock, [this] { return !this->data_.empty(); });
    auto res = std::move(data_.front());
    data_.pop();
    return res;
}

template <MovableType T>
void TSQueue<T>::push(T value) {
    std::lock_guard lock(data_mutex_);
    data_.push(std::move(value));
    data_cond_.notify_one();
}

template <MovableType T>
bool TSQueue<T>::empty() const noexcept {
    std::lock_guard lock(data_mutex_);
    return data_.empty();
}

template <MovableType T>
std::size_t TSQueue<T>::size() const noexcept {
    std::lock_guard lock(data_mutex_);
    return data_.size();
}
}  // namespace ep
