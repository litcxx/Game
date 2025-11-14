#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace ep::utils
{
  template<typename T>
  class TSQueue {
  public:
    TSQueue() = default;
    TSQueue(const TSQueue&) = delete;
    TSQueue& operator=(const TSQueue&) = delete;

    std::shared_ptr<T> TryPop() noexcept;
    bool TryPop(T& value);

    std::shared_ptr<T> WaitAndPop() noexcept;
    void WaitAndPop(T& value);

    void Push(T new_value);
    void Push(std::shared_ptr<T> new_value);
    bool Empty() const noexcept;
    int Size() const noexcept;

  private:
    mutable std::mutex data_mutex_;
    std::condition_variable data_cond_;
    std::queue<std::shared_ptr<T>> data_;
  };

  template<typename T>
  std::shared_ptr<T> TSQueue<T>::TryPop() noexcept
  {
    std::lock_guard lock(data_mutex_);
    if (data_.empty())
      return std::shared_ptr<T>();
    std::shared_ptr<T> res = std::move(data_.front());
    data_.pop();
    return res;
  }

  template<typename T>
  bool TSQueue<T>::TryPop(T& value)
  {
    std::lock_guard lock(data_mutex_);
    if (data_.empty())
      return false;
    value = std::move(*data_.front());
    data_.pop();
    return true;
  }

  template<typename T>
  std::shared_ptr<T> TSQueue<T>::WaitAndPop() noexcept
  {
    std::unique_lock lock(data_mutex_);
    data_cond_.wait(lock, [this]{ return !this->data_.empty(); });
    std::shared_ptr<T> res = std::move(data_.front());
    data_.pop();
    return res;
  }

  template<typename T>
  void TSQueue<T>::WaitAndPop(T& value)
  {
    std::unique_lock lock(data_mutex_);
    data_cond_.wait(lock, [this]{ return !this->data_.empty(); });
    value = std::move(*data_.front());
    data_.pop();
  }

  template<typename T>
  void TSQueue<T>::Push(T new_value)
  {
    std::shared_ptr<T> data = std::make_shared<T>(std::move(new_value));
    std::lock_guard lock(data_mutex_);
    data_.push(data);
    data_cond_.notify_one();
  }

  template<typename T>
  void TSQueue<T>::Push(std::shared_ptr<T> new_value)
  {
    std::lock_guard lock(data_mutex_);
    data_.push(new_value);
    data_cond_.notify_one();
  }

  template<typename T>
  bool TSQueue<T>::Empty() const noexcept
  {
    std::lock_guard lock(data_mutex_);
    return data_.empty();
  }

  template<typename T>
  int TSQueue<T>::Size() const noexcept
  {
    std::lock_guard lock(data_mutex_);
    return data_.size();
  }
}
