#pragma once

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }
    
    RawMemory(const RawMemory&) = delete;
    
    RawMemory& operator=(const RawMemory& rhs) = delete;
    
    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::exchange(rhs.buffer_, nullptr);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }
        return *this;
    }
    
    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    
    Vector() = default;

    explicit Vector(size_t size)
            : data_(size)
            , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
            : data_(other.size_)
            , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    
    Vector(Vector&& other)
            : data_(other.size_)
            , size_(other.size_)  //
    {
        data_.Swap(other.data_);
        other.size_ = 0;
    }
    
    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        } else if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        if (pos == end()) {
            PushBack(value);
            return end() - 1;
        }
        int64_t insert_idx = pos - begin();
        int64_t idx_from_end = end() - pos;
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : Capacity() * 2);
            T* temp_value = new (new_data + insert_idx) T(value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                try {
                    std::uninitialized_move_n(data_.GetAddress(), insert_idx, new_data.GetAddress());
                } catch (...) {
                    temp_value->~T();
                }
                try {
                    std::uninitialized_move_n(data_ + insert_idx, idx_from_end, new_data + insert_idx + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress(), insert_idx + 1);
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            } else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), insert_idx, new_data.GetAddress());
                } catch (...) {
                    temp_value->~T();
                }
                try {
                    std::uninitialized_copy_n(data_ + insert_idx, idx_from_end, new_data + insert_idx + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress(), insert_idx + 1);
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            }
            ++size_;
            return temp_value;
        } else {
            T value_copy(value);
            new (&data_[size_]) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + insert_idx, end() - 1, end());
            data_[insert_idx] = std::move(value_copy);
            ++size_;
            return begin() + insert_idx;
        }
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        if (pos == end()) {
            PushBack(std::move(value));
            return end() - 1;
        }
        int64_t insert_idx = pos - begin();
        int64_t idx_from_end = end() - pos;
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : Capacity() * 2);
            T* temp_value = new (new_data + insert_idx) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                try {
                    std::uninitialized_move_n(data_.GetAddress(), insert_idx, new_data.GetAddress());
                } catch (...) {
                    temp_value->~T();
                }
                try {
                    std::uninitialized_move_n(data_ + insert_idx, idx_from_end, new_data + insert_idx + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress(), insert_idx + 1);
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            } else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), insert_idx, new_data.GetAddress());
                } catch (...) {
                    temp_value->~T();
                }
                try {
                    std::uninitialized_copy_n(data_ + insert_idx, idx_from_end, new_data + insert_idx + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress(), insert_idx + 1);
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            }
            ++size_;
            return temp_value;
        } else {
            T value_copy(std::move(value));
            new (&data_[size_]) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + insert_idx, end() - 1, end());
            data_[insert_idx] = std::move(value_copy);
            ++size_;
            return begin() + insert_idx;
        }
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (pos == end()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }
        int64_t insert_idx = pos - begin();
        int64_t idx_from_end = end() - pos;
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : Capacity() * 2);
            T* temp_value = new (new_data + insert_idx) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                try {
                    std::uninitialized_move_n(data_.GetAddress(), insert_idx, new_data.GetAddress());
                } catch (...) {
                    temp_value->~T();
                }
                try {
                    std::uninitialized_move_n(data_ + insert_idx, idx_from_end, new_data + insert_idx + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress(), insert_idx + 1);
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            } else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), insert_idx, new_data.GetAddress());
                } catch (...) {
                    temp_value->~T();
                }
                try {
                    std::uninitialized_copy_n(data_ + insert_idx, idx_from_end, new_data + insert_idx + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress(), insert_idx + 1);
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            }
            ++size_;
            return temp_value;
        } else {
            T value_copy(std::forward<Args>(args)...);
            new (&data_[size_]) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + insert_idx, end() - 1, end());
            data_[insert_idx] = std::move(value_copy);
            ++size_;
            return begin() + insert_idx;
        }
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* new_value = nullptr;
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : Capacity() * 2);
            new_value = new (&new_data[size_]) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new_value = new (&data_[size_]) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *new_value;
    }
    
    void PushBack(const T& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : Capacity() * 2);
            new (&new_data[size_]) T(value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
                new (&data_[size_]) T(value);
        }
        ++size_;
    }
    
    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data((size_ == 0) ? 1 : Capacity() * 2);
            new (&new_data[size_]) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new (&data_[size_]) T(std::move(value));
        }
        ++size_;
    }
    
    void PopBack()  noexcept {
        data_[size_ - 1].~T();
        --size_;
    }
    
    iterator Erase(const_iterator pos) {
        int64_t erase_idx = pos - begin();
        std::move(begin() + erase_idx + 1, end(), begin() + erase_idx);
        (end() - 1)->~T();
        --size_;
        return begin() + erase_idx;
    }
    
    Vector& operator=(const Vector& rhs) {
        if (rhs.size_ > data_.Capacity()) {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
        } else {
            if (rhs.size_ < size_) {
                for (size_t i = 0; i < rhs.size_; ++i) {
                    data_[i] = rhs.data_[i];
                }
                std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
            } else {
                for (size_t i = 0; i < size_; ++i) {
                    data_[i] = rhs.data_[i];
                }
                std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
            }
            size_ = rhs.size_;
        }
        return *this;
    }
    
    Vector& operator=(Vector&& rhs) {
        data_.Swap(rhs.data_);
        rhs.size_ = 0;
        return *this;
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
private:
    RawMemory<T> data_;
    size_t size_ = 0;
};



