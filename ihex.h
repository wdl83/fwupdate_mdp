#pragma once

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <ostream>
#include <string>
#include <vector>

#include "Ensure.h"

/* Intel HEX format:
 *
 * [StartCode][ByteCount][Address][RecordType][Data][Checksum]
 *
 * [StartCode] : 1x char (fixed ':')
 * [ByteCount] : 2x hex (0-255), number of bytes in [Data] field
 * [Address]   : 4x hex (0x0000 - 0xFFFF), bigendian physical [Data] location
 * [RecordType]: 2x hex (0x00-0x05), define [Data] type
 * [Data]      : Nx hex, sequence of bytes
 * [Checksum]  : 2x hex, LSB 2's complement of [Data] bytes sum */

namespace ihex {

enum class RecordType : uint8_t
{
    Begin = 0x00,
    Data = Begin,
    EndOfFile = 0x01,
    ExtendedSegmentAddr = 0x02,
    StartSegmentAddr = 0x03,
    ExtendedLinearAddr = 0x04,
    StartLinearAddr = 0x05,
    End
};

inline
uint8_t calcChecksum(std::string::const_iterator begin, const std::string::const_iterator end)
{
    uint8_t sum = 0;

    while(begin != end)
    {
        const uint8_t value = uint8_t(std::stoi(std::string(begin, std::next(begin, 2)), 0, 16));
        sum += value;
        std::advance(begin, sizeof(uint8_t) << 1);
    }

    const uint8_t checksum = (~sum) + uint8_t{1};
    return checksum;
}

class Record
{
    friend
    std::ostream &operator<< (std::ostream &os, const Record &record)
    {
        os
            << "addr " << std::hex << std::setw(4) << std::setfill('0')
            << record.addr()
            << " size " << std::hex << std::setw(2) << std::setfill('0')
            << record.size()
            << " (" << std::dec << record.size() << ")";
        return os;
    }

    RecordType type_;
    uint16_t addr_;
    std::vector<uint8_t> data_;
    uint8_t checksum_;
public:
    Record(RecordType type, uint16_t addr, std::vector<uint8_t> data, uint8_t checksum):
        type_{type},
        addr_{addr},
        data_{std::move(data)},
        checksum_{checksum}
    {}

    RecordType type() const {return type_;}
    uint16_t addr() const {return addr_;}
    uint16_t size() const {return data_.size();}
    const std::vector<uint8_t> &data() const {return data_;}
};

Record parseRecord(std::string::const_iterator begin, const std::string::const_iterator end);

} /* ihex */
