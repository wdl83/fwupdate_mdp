#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <thread>

#include <unistd.h>

#include "Ensure.h"
#include "Trace.h"
#include "flash.h"
#include "ihex.h"

#include "modbus_mdp/crc.h"

namespace {

void help(const char *argv0, const char *message = nullptr)
{
    if(message) std::cout << "WARNING: " << message << '\n';

    std::cout
        << argv0
        << " -f filename"
        << std::endl;
}

using RecordSeq = std::vector<ihex::Record>;

RecordSeq parseRecordSeq(std::istream &file)
{
    ENSURE(file, RuntimeError);

    file >> std::noskipws;

    std::string fileData
    {
        std::istream_iterator<char>{file},
        std::istream_iterator<char>{}
    };

    auto curr = std::begin(fileData);
    const auto end = std::end(fileData);
    RecordSeq seq;

    while(curr != end)
    {
        if('\n' == *curr)
        {
            std::advance(curr, 1);
            continue;
        }

        auto i = std::find_if(curr, end, [](char c){return '\n' == c;});

        seq.push_back(ihex::parseRecord(curr, i));
        curr = i;
    }
    return seq;
}

void append(std::vector<uint8_t> &dst, const std::vector<uint8_t> &src)
{
    dst.insert(std::end(dst), std::begin(src), std::end(src));
}

void dump(std::ostream &os, uint8_t data)
{
    os << "0x" << std::hex << std::setw(2) << std::setfill('0') << int(data);
}

void dump(std::ostream &os, const uint8_t *begin, const uint8_t *const end)
{
    const auto flags = os.flags();
    int cntr = 0;

    while(begin != end)
    {
        dump(os, *begin);
        ++begin;
        ++cntr;
        os << ' ';

        if(0 == (cntr % 16)) os << '\n';
    }
    os << '\n';
    os.flags(flags);
}

Modbus::RTU::CRC calcChecksum(const RecordSeq &seq)
{
    std::vector<uint8_t> data;

    for(auto begin = std::begin(seq); std::end(seq) != begin; ++begin)
    {
        if(ihex::RecordType::EndOfFile == begin->type()) break;
        if(ihex::RecordType::Data != begin->type())
        {
            TRACE(TraceLevel::Warning, "skipped ", *begin);
            continue;
        }

        append(data, begin->data());
    }

    //dump(std::cout, data.data(), data.data() + data.size());

    return Modbus::RTU::calcCRC(data.data(), data.data() + data.size());
}

} /* namespace */

int main(int argc, char *const argv[])
{
    std::string fileName;

    for(int c; -1 != (c = ::getopt(argc, argv, "hf:"));)
    {
        switch(c)
        {
            case 'h':
                help(argv[0]);
                return EXIT_SUCCESS;
                break;
            case 'f':
                fileName = optarg ? optarg : "";
                break;
            case ':':
            case '?':
            default:
                help(argv[0], "geopt() failure");
                return EXIT_FAILURE;
                break;
        }
    }

    if(fileName.empty())
    {
        help(argv[0], "missing/invalid required arguments");
        return EXIT_FAILURE;
    }

    try
    {
        std::ifstream file{fileName};
        const auto checksum = calcChecksum(parseRecordSeq(file));

        std::ostringstream oss;

        oss
            << "HEX 0x"
            << std::hex << std::setw(2) << std::setfill('0') << int(checksum.highByte())
            << ",0x"
            << std::hex << std::setw(2) << std::setfill('0') << int(checksum.lowByte())
            << ", DEC "
            << std::dec << int(checksum.highByte())
            << ','
            << std::dec << int(checksum.lowByte());

        TRACE(TraceLevel::Info, "checksum ", oss.str());
    }
    catch(const std::exception &except)
    {
        TRACE(TraceLevel::Error, except.what());
        return EXIT_FAILURE;
    }
    catch(...)
    {
        TRACE(TraceLevel::Error, "unsupported exception");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
