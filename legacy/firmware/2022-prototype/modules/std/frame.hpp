#ifndef STD_FRAME_HPP
#define STD_FRAME_HPP
#include <algorithm>

template <typename T>
class frame
{
public:
    using data_type = T;
    using pointer = T *;
    using reference = T &;
    using const_reference = const T &;

public:
    frame() noexcept = default;

    /**
     * @brief Fill constructor
     * 
     * @param size size of the block
     * @param value value to fill
     */
    frame(size_t size, const_reference value) noexcept
    {
        block_start = new data_type[size];
        block_end = block_start + size;
        std::fill(block_start, block_end, value);
        head = block_start;
    }

    ~frame() noexcept
    {
        delete[] block_start;
    }

    frame(const frame &other) noexcept
    {
        block_start = new data_type[other.block_end - other.block_start];
        block_end = block_start + (other.block_end - other.block_start);
        std::copy(other.block_start, other.block_end, block_start);
        head = block_start;
    }

    frame(frame &&other) noexcept
    {
        block_start = other.block_start;
        block_end = other.block_end;
        head = other.head;
        other.block_start = nullptr;
        other.block_end = nullptr;
        other.head = nullptr;
    }

    constexpr size_t size() const noexcept
    {
        return block_end - block_start;
    }

    reference operator[] (__decltype(1) index)
    {
        pointer p = head + index;
        while (p >= block_end)
            p -= size();
        while (p < block_start)
            p += size();
        return *p;
    }

    const_reference operator[] (__decltype(1) index) const
    {
        pointer p = head + index;
        while (p >= block_end)
            p -= size();
        while (p < block_start)
            p += size();

        return *p;
    }

    void push_back(const_reference value)
    {
        ++head;
        if (head == block_end)
            head = block_start;
        *head = value;
    }

    data_type avg() const
    {
        data_type tot {};
        for (auto *ptr = block_start; ptr != block_end; ++ptr)
            tot += *ptr;
        return tot / size();
    }

    data_type min() const
    {
        return std::min_element(block_start, block_end);
    }

    data_type max() const
    {
        return std::max_element(block_start, block_end);
    }
    

public:
    pointer block_start{}, block_end{}, head{};
};

#endif
