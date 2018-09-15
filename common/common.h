/*
 * common.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_COMMON_H_
#define ENERGIMINER_COMMON_H_

#include "Log.h"
#include "portable_endian.h"
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <atomic>
#include <array>

template <class GuardType, class MutexType>
struct GenericGuardBool: GuardType
{
	GenericGuardBool(MutexType& _m): GuardType(_m) {}
	bool b = true;
};

#define DEV_GUARDED(MUTEX) \
	for (GenericGuardBool<std::lock_guard<std::mutex>, std::mutex> __eth_l(MUTEX); __eth_l.b; __eth_l.b = false)

enum class MinerExecutionMode : unsigned
{
    kCPU   = 0x1,
    kCL    = 0x2,
    kMixed = 0x3,
    kCUDA  = 0x4
};

enum class EnumMinerEngine : unsigned
{
    kCPU    = 0x0,
    kCL     = 0x1,
    kCUDA   = 0x2,
    kTest   = 0x4
};

enum class HwMonitorInfoType
{
    UNKNOWN,
    NVIDIA,
    AMD
};

enum class HwMonitorIndexSource
{
    UNKNOWN,
    OPENCL,
    CUDA
};

struct HwMonitorInfo
{
    HwMonitorInfoType deviceType = HwMonitorInfoType::UNKNOWN;
    HwMonitorIndexSource indexSource = HwMonitorIndexSource::UNKNOWN;
    int deviceIndex = -1;

};

struct HwMonitor
{
    int tempC = 0;
    int fanP = 0;
    double powerW = 0;
};

inline std::ostream& operator<<(std::ostream& os, const HwMonitor& _hw)
{
    os << _hw.tempC << "C " << _hw.fanP << "%";
    if(_hw.powerW) {
        os << ' ' << std::fixed << std::setprecision(0) << _hw.powerW << "W";
    }
    return os;
}


uint16_t inline ReadLE16(const unsigned char* ptr)
{
    return le16toh(*((uint16_t*)ptr));
}

uint32_t inline ReadLE32(const unsigned char* ptr)
{
    return le32toh(*((uint32_t*)ptr));
}

uint64_t inline ReadLE64(const unsigned char* ptr)
{
    return le64toh(*((uint64_t*)ptr));
}

void inline WriteLE16(unsigned char* ptr, uint16_t x)
{
    *((uint16_t*)ptr) = htole16(x);
}

void inline WriteLE32(unsigned char* ptr, uint32_t x)
{
    *((uint32_t*)ptr) = htole32(x);
}

void inline WriteLE64(unsigned char* ptr, uint64_t x)
{
    *((uint64_t*)ptr) = htole64(x);
}

uint32_t inline ReadBE32(const unsigned char* ptr)
{
    return be32toh(*((uint32_t*)ptr));
}

uint64_t inline ReadBE64(const unsigned char* ptr)
{
    return be64toh(*((uint64_t*)ptr));
}

void inline WriteBE32(unsigned char* ptr, uint32_t x)
{
    *((uint32_t*)ptr) = htobe32(x);
}

void inline WriteBE64(unsigned char* ptr, uint64_t x)
{
    *((uint64_t*)ptr) = htobe64(x);
}

/// Converts arbitrary value to string representation using std::stringstream.
template <class _T>
std::string toString(_T const& _t)
{
    std::ostringstream o;
    o << _t;
    return o.str();
}

inline std::vector<EnumMinerEngine> getEngineModes(MinerExecutionMode minerExecutionMode)
{
    std::vector<EnumMinerEngine> vEngine;
    if ( static_cast<unsigned>(minerExecutionMode) & static_cast<unsigned>(MinerExecutionMode::kCL) )
        vEngine.push_back(EnumMinerEngine::kCL);
    if ( static_cast<unsigned>(minerExecutionMode) & static_cast<unsigned>(MinerExecutionMode::kCPU) )
        vEngine.push_back(EnumMinerEngine::kCPU);
    if (static_cast<unsigned>(minerExecutionMode) & static_cast<unsigned>(MinerExecutionMode::kCUDA))
        vEngine.push_back(EnumMinerEngine::kCUDA);

    return vEngine;
}

inline EnumMinerEngine getEngineMode(MinerExecutionMode minerExecutionMode)
{
    if ( static_cast<unsigned>(minerExecutionMode) & static_cast<unsigned>(MinerExecutionMode::kCL) )
        return EnumMinerEngine::kCL;
    if ( static_cast<unsigned>(minerExecutionMode) & static_cast<unsigned>(MinerExecutionMode::kCPU) )
        return EnumMinerEngine::kCPU;
    if (static_cast<unsigned>(minerExecutionMode) & static_cast<unsigned>(MinerExecutionMode::kCUDA))
        return EnumMinerEngine::kCUDA;

    return EnumMinerEngine::kTest;
}

constexpr const char* StrMinerEngine[] = { "CPU", "CL", "CUDA", "Test" };

inline std::string to_string(EnumMinerEngine minerEngine)
{
        return StrMinerEngine[static_cast<int>(minerEngine)];
}

namespace energi {

inline void setBuffer(const uint8_t* ptr, uint32_t value)
{
    *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(ptr)) = value;
}


inline std::string strToHex(const std::string& str)
{
    std::ostringstream result;
    for (const auto& item : str) {
        result << std::hex
              << std::setfill('0')
              << std::setw(2)
              << std::nouppercase
              << static_cast<unsigned int>(static_cast<unsigned char>(item));
    }
    return result.str();
}

inline bool setenv(const char name[], const char value[], bool over = false)
{
#if _WIN32
    if (!over && std::getenv(name) != nullptr)
        return true;

    return ::_putenv_s(name, value) == 0;
#else
    return ::setenv(name, value, over ? 1 : 0) == 0;
#endif
}

class WorkException : public std::exception
{
public:
    WorkException(const std::string &reason)
        : m_reason(reason)
    {
    }
    const char* what() const noexcept override
    {
        return m_reason.c_str();
    }
private:
    std::string m_reason;
};

/// Pause mining
typedef enum
{
    MINING_NOT_PAUSED              = 0x00000000,
    MINING_PAUSED_WAIT_FOR_T_START = 0x00000001,
    MINING_PAUSED_API              = 0x00000002
    // MINING_PAUSED_USER             = 0x00000004,
    // MINING_PAUSED_ERROR            = 0x00000008
} MinigPauseReason;

struct MiningPause
{
    std::atomic<uint64_t> m_mining_paused_flag = {MinigPauseReason::MINING_NOT_PAUSED};

    void set_mining_paused(MinigPauseReason pause_reason)
    {
        m_mining_paused_flag.fetch_or(pause_reason, std::memory_order_seq_cst);
    }

    void clear_mining_paused(MinigPauseReason pause_reason)
    {
        m_mining_paused_flag.fetch_and(~pause_reason, std::memory_order_seq_cst);
    }

    MinigPauseReason get_mining_paused()
    {
        return (MinigPauseReason) m_mining_paused_flag.load(std::memory_order_relaxed);
    }

    bool is_mining_paused() const
    {
        return (m_mining_paused_flag.load(std::memory_order_relaxed) != MinigPauseReason::MINING_NOT_PAUSED);
    }
};

/// Describes the progress of a mining operation.
struct WorkingProgress
{
    float hashRate = 0.0;
    std::map<std::string, float> minersHashRates; // maps a miner's device name to it's hash count
    std::map<std::string, bool> miningIsPaused;
    std::map<std::string, HwMonitor> minerMonitors;

};

inline std::ostream& operator<<(std::ostream& _out, WorkingProgress _p)
{
    float mh = _p.hashRate / 1000000.0f;
    _out << "Speed "
         << EthTealBold << std::fixed << std::setprecision(2) << mh << EthReset
        << " Mh/s    ";
    for (auto const & i : _p.minersHashRates) {
        auto pauseIter = _p.miningIsPaused.find(i.first);
        if (pauseIter != _p.miningIsPaused.end()) {
            if (pauseIter->second) {
                _out << EthRed;
            }
        }
        mh = _p.minersHashRates[i.first] / 1000000.0f;
        _out << i.first << " " << EthTeal << std::fixed << std::setprecision(2) << mh << EthReset << "  ";
        auto iter = _p.minerMonitors.find(i.first);
        if (iter != _p.minerMonitors.end()) {
            _out << " " << EthTeal << _p.minerMonitors[i.first] << EthReset << "  ";
        }
    }
    return _out;
}

inline std::string GetHex(const uint8_t* data, unsigned int size)
{
    std::string psz(size * 2 + 1, '\0');
    for (unsigned int i = 0; i < size; i++)
        sprintf(const_cast<char*>(psz.data()) + i * 2, "%02x", data[size - i - 1]);
    return std::string(const_cast<char*>(psz.data()), const_cast<char*>(psz.data()) + size * 2);
}


#if ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define WANT_BUILTIN_BSWAP
#else
#define bswap_32(x) ((((x) << 24) & 0xff000000u) | (((x) << 8) & 0x00ff0000u) \
        | (((x) >> 8) & 0x0000ff00u) | (((x) >> 24) & 0x000000ffu))
#endif

static inline uint32_t swab32(uint32_t v)
{
#ifdef WANT_BUILTIN_BSWAP
    return __builtin_bswap32(v);
#else
    return bswap_32(v);
#endif
}

#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

}


#endif /* ENERGIMINER_COMMON_H_ */
