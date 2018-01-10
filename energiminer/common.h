/*
 * common.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_COMMON_H_
#define ENERGIMINER_COMMON_H_

#include "energiminer/Log.h"
#include "portable_endian.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <array>



namespace energi
{
  using target    = std::array<uint32_t, 8>;
  using vchar     = std::vector<char>;
  using vbyte     = std::vector<uint8_t>;
  using vuint32   = std::vector<uint32_t>;

  inline void setBuffer(const uint8_t* ptr, uint32_t value) { *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(ptr)) = value; }

  using MutexLGuard = std::lock_guard<std::mutex>;
  using MutexRLGuard = std::lock_guard<std::recursive_mutex>;


  inline bool setenv(const char name[], const char value[], bool override = false)
  {
  #if _WIN32
    if (!override && std::getenv(name) != nullptr)
      return true;

    return ::_putenv_s(name, value) == 0;
  #else
    return ::setenv(name, value, override ? 1 : 0) == 0;
  #endif
  }

  class WorkException : public std::exception
  {
  public:
    WorkException(const std::string &reason):m_reason(reason){}
      const char* what() const _GLIBCXX_USE_NOEXCEPT override
      {
          return m_reason.c_str();
      }
  private:
      std::string m_reason;
  };

  inline std::string GetHex(const uint8_t* data, unsigned int size)
  {
      std::string psz(size * 2 + 1, '\0');
      for (unsigned int i = 0; i < size; i++)
          sprintf(const_cast<char*>(psz.data()) + i * 2, "%02x", data[size - i - 1]);
      return std::string(const_cast<char*>(psz.data()), const_cast<char*>(psz.data()) + size * 2);
  }

  #pragma pack(push, 1)

  //*
  //*   The Keccak-256 hash of this structure is used as input for egihash
  //*   It is a truncated block header with a deterministic encoding
  //*   All integer values are little endian
  //*   Hashes are the nul-terminated hex encoded representation as if ToString() was called

  // Bytes => 4 + 65 + 65 + 4 + 4 + 4 -> 130 + 16 -> 146 bytes
  struct CBlockHeaderTruncatedLE
  {
    int32_t nVersion;
    char hashPrevBlock[65];
    char hashMerkleRoot[65];
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nHeight;

    CBlockHeaderTruncatedLE(const void* data)
    : nVersion(htole32(*reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(data) + 0)))
    , hashPrevBlock{0}
    , hashMerkleRoot{0}
    , nTime(htole32(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(data) + 4 + 32 + 32 )))
    , nBits(htole32(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(data) + 4 + 32 + 32 + 4 )))
    , nHeight(htole32(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(data) + 4 + 32 + 32 + 4 + 4 )))
    {
      auto inputHashPrevBlock = reinterpret_cast<const uint8_t*>(data) + 4;
      auto prevHash = GetHex(inputHashPrevBlock, 32);
      std::memcpy(hashPrevBlock, prevHash.c_str(), (std::min)(prevHash.size(), sizeof(hashPrevBlock)));

      auto inputMerkleHashPrevBlock = reinterpret_cast<const uint8_t*>(data) + 36;
      auto merkleRoot = GetHex(inputMerkleHashPrevBlock, 32);
      std::memcpy(hashMerkleRoot, merkleRoot.c_str(), (std::min)(merkleRoot.size(), sizeof(hashMerkleRoot)));
    }
  };

  static_assert(sizeof(CBlockHeaderTruncatedLE) == 146, "CBlockHeaderTruncatedLE has incorrect size");

  struct CBlockHeaderFullLE : public CBlockHeaderTruncatedLE
  {
      uint32_t nNonce;
      char hashMix[65];

      CBlockHeaderFullLE(const void* data, uint32_t nonce, const uint8_t* hashMix_)
      : CBlockHeaderTruncatedLE(data)
      , nNonce(nonce)
      , hashMix{0}
      {
          auto mixString = GetHex(hashMix_, 32);
          /*cdebug << "NONCE:" << nNonce;
          cdebug << "GET HEX HASH:" << mixString;*/
          std::memcpy(hashMix, mixString.c_str(), (std::min)(mixString.size(), sizeof(hashMix)));
      }
  };
  static_assert(sizeof(CBlockHeaderFullLE) == 215, "CBlockHeaderFullLE has incorrect size");
  #pragma pack(pop)




  /// Describes the progress of a mining operation.
  struct WorkingProgress
  {
    uint64_t hashes = 0;    ///< Total number of hashes computed.
    uint64_t ms = 0;        ///< Total number of milliseconds of mining thus far.
    uint64_t rate() const { return ms == 0 ? 0 : hashes * 1000 / ms; }

    std::vector<uint64_t> minersHashes;
    uint64_t minerRate(const uint64_t hashCount) const { return ms == 0 ? 0 : hashCount * 1000 / ms; }
  };

  inline std::ostream& operator<<(std::ostream& _out, WorkingProgress _p)
  {
    float mh = _p.rate() / 1000.0f;
    _out << "Speed "
       << EthTealBold << std::fixed << std::setw(6) << std::setprecision(2) << mh << EthReset
       << " Kh/s    ";

    for (size_t i = 0; i < _p.minersHashes.size(); ++i)
    {
      mh = _p.minerRate(_p.minersHashes[i]) / 1000.0f;
      _out << "device/" << i << " " << EthTeal << std::fixed << std::setw(5) << std::setprecision(2) << mh << EthReset << "  ";
    }

    return _out;
  }

  static const char b58digits[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

  bool b58dec(unsigned char *bin, size_t binsz, const char *b58);
  int b58check(unsigned char *bin, size_t binsz, const char *b58);
  size_t address_to_script(unsigned char *out, size_t outsz, const char *addr);
  int varint_encode(unsigned char *p, uint64_t n);
  void bin2hex(char *s, const unsigned char *p, size_t len);
  char *abin2hex(const unsigned char *p, size_t len);
  bool hex2bin(unsigned char *p, const char *hexstr, size_t len);
  bool fulltest(const uint32_t *hash, const uint32_t *target);
  extern "C" void sha256d(unsigned char *hash, const unsigned char *data, int len);


  #define _ALIGN(x) __attribute__ ((aligned(x)))

  static inline bool is_windows(void) {
  #ifdef WIN32
    return 1;
  #else
    return 0;
  #endif
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

  typedef unsigned char uchar;

  #if !HAVE_DECL_BE32DEC
  static inline uint32_t be32dec(const void *pp)
  {
    const uint8_t *p = (uint8_t const *)pp;
    return ((uint32_t)(p[3]) + ((uint32_t)(p[2]) << 8) +
        ((uint32_t)(p[1]) << 16) + ((uint32_t)(p[0]) << 24));
  }
  #endif

  #if !HAVE_DECL_LE32DEC
  static inline uint32_t le32dec(const void *pp)
  {
    const uint8_t *p = (uint8_t const *)pp;
    return ((uint32_t)(p[0]) + ((uint32_t)(p[1]) << 8) +
        ((uint32_t)(p[2]) << 16) + ((uint32_t)(p[3]) << 24));
  }
  #endif

  #if !HAVE_DECL_BE32ENC
  static inline void be32enc(void *pp, uint32_t x)
  {
    uint8_t *p = (uint8_t *)pp;
    p[3] = x & 0xff;
    p[2] = (x >> 8) & 0xff;
    p[1] = (x >> 16) & 0xff;
    p[0] = (x >> 24) & 0xff;
  }
  #endif

  #if !HAVE_DECL_LE32ENC
  static inline void le32enc(void *pp, uint32_t x)
  {
    uint8_t *p = (uint8_t *)pp;
    p[0] = x & 0xff;
    p[1] = (x >> 8) & 0xff;
    p[2] = (x >> 16) & 0xff;
    p[3] = (x >> 24) & 0xff;
  }
  #endif

  #if !HAVE_DECL_LE16DEC
  static inline uint16_t le16dec(const void *pp)
  {
    const uint8_t *p = (uint8_t const *)pp;
    return ((uint16_t)(p[0]) + ((uint16_t)(p[1]) << 8));
  }
  #endif

  #if !HAVE_DECL_LE16ENC
  static inline void le16enc(void *pp, uint16_t x)
  {
    uint8_t *p = (uint8_t *)pp;
    p[0] = x & 0xff;
    p[1] = (x >> 8) & 0xff;
  }
  #endif
}


#endif /* ENERGIMINER_COMMON_H_ */
