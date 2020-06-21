#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

#include <unistd.h>

#include <nlohmann/json.hpp>

#include "Ensure.h"
#include "Trace.h"
#include "flash.h"
#include "ihex.h"
#include "mdp/Client.h"

namespace {

template <typename T, typename V>
bool inRange(V value)
{
    return
        std::numeric_limits<T>::min() <= value
        && std::numeric_limits<T>::max() >= value;
}

uint8_t lowByte(uint16_t word)
{
    return word & 0xFF;
}

uint8_t highByte(uint16_t word)
{
    return word >> 8;
}

using json = nlohmann::json;

const char *const ADDR = "addr";
const char *const COUNT = "count";
const char *const FCODE = "fcode";
const char *const SLAVE = "slave";
const char *const TIMEOUT_MS = "timeout_ms";
const char *const VALUE = "value";

constexpr auto FCODE_RD_HOLDING_REGISTERS = 3;
constexpr auto FCODE_WR_REGISTER = 6;
constexpr auto FCODE_WR_REGISTERS = 16;
constexpr auto FCODE_RD_BYTES = 65;
constexpr auto FCODE_WR_BYTES = 66;

constexpr uint8_t FLAG_FLASH_PAGE_UPDATE = 0x01;
constexpr uint8_t FLAG_WATCHDOG_DISABLE = 0x10;
constexpr uint8_t FLAG_WATCHDOG_RESET = 0x20;
constexpr uint8_t FLAG_REBOOT = 0x80;

void help(const char *argv0, const char *message = nullptr)
{
    if(message) std::cout << "WARNING: " << message << '\n';

    std::cout
        << argv0
        << " -a broker_address"
        << " -s service_name"
        << " -f filename"
        << " -t slaveID"
        << std::endl;
}

using RecordSeq = std::vector<ihex::Record>;
using FlashPageSeq = std::vector<FlashPage>;

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

FlashPageSeq toFlashPageSeq(
    RecordSeq::const_iterator recordBegin,
    const RecordSeq::const_iterator recordEnd)
{
    // flash page size 64 words (128 bytes)
    const uint16_t flashPageSize = 128;
    FlashPageSeq flashPageSeq;

    for(auto currRecord = recordBegin; currRecord != recordEnd; ++currRecord)
    {
        if(ihex::RecordType::EndOfFile == currRecord->type()) break;
        if(ihex::RecordType::Data != currRecord->type())
        {
            TRACE(TraceLevel::Warning, "skipped ", *currRecord);
            continue;
        }

        const auto dataBegin = std::begin(currRecord->data());
        const auto dataEnd = std::end(currRecord->data());
        auto dataCurr = dataBegin;

        while(dataCurr != dataEnd)
        {
            const auto offset = std::size_t(std::distance(dataBegin, dataCurr));

            if(uint16_t{0} == ((currRecord->addr() + offset) % flashPageSize))
            {
                if(!flashPageSeq.empty())
                {
                    /* make sure prev. page is filled */
                    ENSURE(
                        flashPageSeq.back().capacity() == flashPageSeq.back().size(),
                        RuntimeError);
                }

                flashPageSeq.emplace_back(flashPageSize, currRecord->addr() + offset);
            }

            ENSURE(!flashPageSeq.empty(), RuntimeError);

            auto &currFlashPage = flashPageSeq.back();
            /* data to be appended should belong to current flash page */
            ENSURE(
                currFlashPage.addr() <= currRecord->addr() + offset,
                RuntimeError);
            ENSURE(
                currFlashPage.addr() + currFlashPage.capacity() > currRecord->addr() + offset,
                RuntimeError);

            currFlashPage.append(*dataCurr);
            ++dataCurr;
        }
    }
    return flashPageSeq;
}

constexpr const uint16_t RTU_ADDR_BASE{0x2000};

json toModbusRequest(const FlashPage &flashPage, uint8_t slaveID)
{
    json req
    {
        {
            {SLAVE, slaveID},
            {FCODE, FCODE_WR_BYTES},
            {ADDR, RTU_ADDR_BASE + 3},
            {COUNT, sizeof(uint16_t)},
            {
                VALUE,
                std::vector<uint8_t>
                {
                    /* flash_page_addr is uint16_t little-endian */
                    lowByte(flashPage.addr()),
                    highByte(flashPage.addr())
                }
            }
        },
        {
            {SLAVE, slaveID},
            {FCODE, FCODE_WR_BYTES},
            {TIMEOUT_MS, 1000},
            {ADDR, RTU_ADDR_BASE + 5},
            {COUNT, flashPage.size()},
            {VALUE, flashPage.data()}
        }
    };

    return req;
}

void validateReply(const json &request, const json &reply)
{
    // request and reply should be arrays of same length
    ENSURE(reply.is_array(), RuntimeError);
    ENSURE(request.size() == reply.size(), RuntimeError);
    ENSURE(reply[0].count(SLAVE), RuntimeError);
    ENSURE(request[0][SLAVE] == reply[0][SLAVE], RuntimeError);
}

void handleFlashPageFill(
    const std::string &brokerAddress,
    const std::string &serviceName,
    uint8_t slaveID,
    const FlashPage &flashPage)
{
    auto request = toModbusRequest(flashPage, slaveID);
    auto requestPayload = std::string{request.dump()};

    Client client;

    const auto replyPayload =
        client.exec(brokerAddress, serviceName, {std::move(requestPayload)});

    ENSURE(2 == int(replyPayload.size()), RuntimeError);
    ENSURE(MDP::Broker::Signature::statusSucess == replyPayload[0], RuntimeError);

    validateReply(request, json::parse(replyPayload.back()));
}

void handleFlashPageUpdate(
    const std::string &brokerAddress,
    const std::string &serviceName,
    uint8_t slaveID)
{
    json request
    {
        {
            {SLAVE, slaveID},
            {FCODE, FCODE_WR_BYTES},
            {ADDR, RTU_ADDR_BASE + 0},
            {COUNT, 1},
            {VALUE,  std::vector<uint8_t>{FLAG_FLASH_PAGE_UPDATE}}
        }
    };

    auto requestPayload = std::string{request.dump()};

    Client client;

    const auto replyPayload =
        client.exec(brokerAddress, serviceName, {std::move(requestPayload)});

    ENSURE(2 == int(replyPayload.size()), RuntimeError);
    ENSURE(MDP::Broker::Signature::statusSucess == replyPayload[0], RuntimeError);

    validateReply(request, json::parse(replyPayload.back()));
}

uint16_t fetchFlashPageUpdatedNum(
    const std::string &brokerAddress,
    const std::string &serviceName,
    uint8_t slaveID)
{
    json request
    {
        {
            {SLAVE, slaveID},
            {FCODE, FCODE_RD_BYTES},
            {ADDR, RTU_ADDR_BASE + 2},
            {COUNT, 1}
        }
    };

    auto requestPayload = std::string{request.dump()};

    Client client;

    const auto replyPayload =
        client.exec(brokerAddress, serviceName, {std::move(requestPayload)});

    ENSURE(2 == int(replyPayload.size()), RuntimeError);
    ENSURE(MDP::Broker::Signature::statusSucess == replyPayload[0], RuntimeError);

    const auto reply = json::parse(replyPayload.back());

    validateReply(request, reply);

    ENSURE(reply[0][VALUE].is_array(), RuntimeError);
    ENSURE(1 == reply[0][VALUE].size(), RuntimeError);

    const auto value = reply[0][VALUE][0].get<int>();

    ENSURE(inRange<uint16_t>(value), RuntimeError);

    return uint16_t(value);
}

void handleWatchdogReset(
    const std::string &brokerAddress,
    const std::string &serviceName,
    uint8_t slaveID)
{
    json request
    {
        {
            {SLAVE, slaveID},
            {FCODE, FCODE_WR_BYTES},
            {ADDR, RTU_ADDR_BASE + 0},
            {COUNT, 1},
            {VALUE,  std::vector<uint8_t>{FLAG_WATCHDOG_RESET}}
        }
    };

    auto requestPayload = std::string{request.dump()};

    Client client;

    const auto replyPayload =
        client.exec(brokerAddress, serviceName, {std::move(requestPayload)});

    ENSURE(2 == int(replyPayload.size()), RuntimeError);
    ENSURE(MDP::Broker::Signature::statusSucess == replyPayload[0], RuntimeError);

    validateReply(request, json::parse(replyPayload.back()));
}

void handleReboot(
    const std::string &brokerAddress,
    const std::string &serviceName,
    uint8_t slaveID)
{
    json request
    {
        {
            {SLAVE, slaveID},
            {FCODE, FCODE_WR_BYTES},
            {ADDR, RTU_ADDR_BASE + 0},
            {COUNT, 1},
            {VALUE,  std::vector<uint8_t>{FLAG_REBOOT}}
        }
    };

    auto requestPayload = std::string{request.dump()};

    Client client;

    const auto replyPayload =
        client.exec(brokerAddress, serviceName, {std::move(requestPayload)});

    ENSURE(2 == int(replyPayload.size()), RuntimeError);
    ENSURE(MDP::Broker::Signature::statusSucess == replyPayload[0], RuntimeError);

    validateReply(request, json::parse(replyPayload.back()));
}

void firmwareUpdate(
    std::ifstream &file,
    const std::string &brokerAddress,
    const std::string &serviceName,
    uint8_t slaveID)
{
    auto recordSeq = parseRecordSeq(file);
    auto flashPageSeq = toFlashPageSeq(std::begin(recordSeq), std::end(recordSeq));

    try
    {
        uint16_t flashPageUpdatedNum = 0;

        TRACE(TraceLevel::Info, "ihex ", recordSeq.size(), " records");
        TRACE(TraceLevel::Info, "flush ", flashPageSeq.size(), " pages");

        for(const auto &flashPage : flashPageSeq)
        {
            TRACE(TraceLevel::Debug, "flashing page[", flashPageUpdatedNum, "] ", flashPage);

            try
            {
               ENSURE(
                   flashPageUpdatedNum
                   == fetchFlashPageUpdatedNum(brokerAddress, serviceName, slaveID),
                   RuntimeError);

                handleWatchdogReset(brokerAddress, serviceName, slaveID);
                handleFlashPageFill(brokerAddress, serviceName, slaveID, flashPage);
                handleFlashPageUpdate(brokerAddress, serviceName, slaveID);
                ++flashPageUpdatedNum;
            }
            catch(std::exception &except)
            {
                std::cerr
                    << "std exception " << except.what()
                    << " while flashing " << flashPage << std::endl;
                throw;
            }
        }

        TRACE(TraceLevel::Info, "rebooting");
        handleReboot(brokerAddress, serviceName, slaveID);
    }
    catch(std::exception &except)
    {
        std::cerr << "std exception " << except.what();
        throw;
    }
}

} /* namespace */

int main(int argc, char *const argv[])
{
    std::string brokerAddress;
    std::string serviceName;
    std::string fileName;
    int slaveID = -1;

    for(char c; -1 != (c = ::getopt(argc, argv, "ha:s:f:t:"));)
    {
        switch(c)
        {
            case 'h':
                help(argv[0]);
                return EXIT_SUCCESS;
                break;
            case 'a':
                brokerAddress = optarg ? optarg : "";
                break;
            case 's':
                serviceName = optarg ? optarg : "";
                break;
            case 'f':
                fileName = optarg ? optarg : "";
                break;
            case 't':
                slaveID = optarg ? ::atoi(optarg) : -1;
                break;
            case ':':
            case '?':
            default:
                help(argv[0], "geopt() failure");
                return EXIT_FAILURE;
                break;
        }
    }

    if(
        brokerAddress.empty()
        || serviceName.empty()
        || fileName.empty()
        || slaveID < 1
        || slaveID > 255)
    {
        help(argv[0], "missing/invalid required arguments");
        return EXIT_FAILURE;
    }

    try
    {
        std::ifstream file{fileName};
        firmwareUpdate(file, brokerAddress, serviceName, slaveID);
    }
    catch(const std::exception &except)
    {
        std::cerr << "std exception " << except.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch(...)
    {
        std::cerr << "unsupported exception" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
