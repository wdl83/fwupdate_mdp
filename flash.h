#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "Ensure.h"

class FlashPage
{
    friend
    std::ostream &operator<< (std::ostream &os, const FlashPage &flashPage)
    {
        os
            << "addr " << std::hex << std::setw(4) << std::setfill('0')
            << flashPage.addr()
            << " size " << std::hex << std::setw(2) << std::setfill('0')
            << flashPage.size()
            << " (" << std::dec << flashPage.size() << ")";
#if 0
        for(const auto data : flashPage.data())
        {
            os << std::hex << std::setw(2) << std::setfill('0') << int(data);
        }
#endif
        return os;
    }

    std::size_t capacity_;
    uint16_t addr_;
    std::vector<uint8_t> data_;
public:
    FlashPage(uint16_t capacity, uint16_t addr):
        capacity_{capacity},
        addr_{addr}
    {}

    void append(uint8_t byte)
    {
        ENSURE(capacity() > data_.size(), RuntimeError);
        data_.push_back(byte);
    }

    template <typename I>
    void append(I begin, const I end)
    {
        const auto num = std::distance(begin, end);

        ENSURE(capacity() >= data_.size() + num, RuntimeError);

        data_.insert(std::end(data_), begin, end);
    }

    std::size_t capacity() const {return capacity_;}
    uint16_t addr() const {return addr_;}
    std::size_t size() const {return data_.size();}
    const std::vector<uint8_t> &data() const {return data_;}
};
