#include "ihex.h"

namespace ihex {

Record parseRecord(std::string::const_iterator begin, const std::string::const_iterator end)
{
    auto curr = begin;
    /* make sure its single line */
    ENSURE(
        std::all_of(
            begin, end,
            [](char c){return '\n' != c;}),
        RuntimeError);

    // StartCode
    ENSURE(curr != end, RuntimeError);
    ENSURE(':' == *curr, RuntimeError);
    std::advance(curr, sizeof(char));

    // ByteCount
    ENSURE(1 < std::distance(curr, end), RuntimeError);
    const uint8_t byteCount = uint8_t(std::stoi(std::string(curr, std::next(curr, 2)), 0, 16));
    std::advance(curr, sizeof(byteCount) << 1);

    // Address
    ENSURE(3 < std::distance(curr, end), RuntimeError);
    const uint16_t addr = uint16_t(std::stoi(std::string(curr, std::next(curr, 4)), 0, 16));
    std::advance(curr, sizeof(addr) << 1);

    // RecordType
    ENSURE(1 < std::distance(curr, end), RuntimeError);
    RecordType recordType = RecordType(std::stoi(std::string(curr, std::next(curr, 2)), 0, 16));
    std::advance(curr, sizeof(recordType) << 1);

    // Data
    ENSURE((byteCount << 1) < std::distance(curr, end), RuntimeError);
    std::vector<uint8_t> data(byteCount, 0);

    for(auto &byte : data)
    {
        byte = uint8_t(std::stoi(std::string(curr, std::next(curr, 2)), 0, 16));
        std::advance(curr, sizeof(byteCount) << 1);
    }

    // Checksum
    ENSURE(1 < std::distance(curr, end), RuntimeError);
    const uint8_t checksum = uint8_t(std::stoi(std::string(curr, std::next(curr, 2)), 0, 16));

    ENSURE(checksum == calcChecksum(std::next(begin), curr), RuntimeError);

    std::advance(curr, sizeof(checksum) << 1);

    return {recordType, addr, std::move(data), checksum};
}

} /* ihex */
