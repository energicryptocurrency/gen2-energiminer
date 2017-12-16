/*
 * common.cpp
 *
 *  Created on: Dec 16, 2017
 *      Author: ranjeet
 */

#include "common.h"

namespace energi
{
  bool b58dec(unsigned char *bin, size_t binsz, const char *b58)
  {
      size_t i, j;
      uint64_t t;
      uint32_t c;
      uint32_t *outi;
      size_t outisz = (binsz + 3) / 4;
      int rem = binsz % 4;
      uint32_t remmask = 0xffffffff << (8 * rem);
      size_t b58sz = strlen(b58);
      bool rc = false;

      outi = (uint32_t *) calloc(outisz, sizeof(*outi));

      for (i = 0; i < b58sz; ++i) {
          for (c = 0; b58digits[c] != b58[i]; c++)
              if (!b58digits[c])
                  goto out;
          for (j = outisz; j--; ) {
              t = (uint64_t)outi[j] * 58 + c;
              c = t >> 32;
              outi[j] = t & 0xffffffff;
          }
          if (c || outi[0] & remmask)
              goto out;
      }

      j = 0;
      switch (rem) {
          case 3:
              *(bin++) = (outi[0] >> 16) & 0xff;
          case 2:
              *(bin++) = (outi[0] >> 8) & 0xff;
          case 1:
              *(bin++) = outi[0] & 0xff;
              ++j;
          default:
              break;
      }
      for (; j < outisz; ++j) {
          be32enc((uint32_t *)bin, outi[j]);
          bin += sizeof(uint32_t);
      }

      rc = true;
      out:
      free(outi);
      return rc;
  }

  int b58check(unsigned char *bin, size_t binsz, const char *b58)
  {
      unsigned char buf[32];
      int i;

      sha256d(buf, bin, (int) (binsz - 4));
      if (memcmp(&bin[binsz - 4], buf, 4))
          return -1;

      /* Check number of zeros is correct AFTER verifying checksum
       * (to avoid possibility of accessing the string beyond the end) */
      for (i = 0; bin[i] == '\0' && b58[i] == '1'; ++i);
      if (bin[i] == '\0' || b58[i] == '1')
          return -3;

      return bin[0];
  }


  size_t address_to_script(unsigned char *out, size_t outsz, const char *addr)
  {
      unsigned char addrbin[25];
      int addrver;
      size_t rv;

      if (!b58dec(addrbin, sizeof(addrbin), addr))
          return 0;
      addrver = b58check(addrbin, sizeof(addrbin), addr);
      if (addrver < 0)
          return 0;
      switch (addrver) {
          case 5:    /* Bitcoin script hash */
          case 196:  /* Testnet script hash */
              if (outsz < (rv = 23))
                  return rv;
              out[ 0] = 0xa9;  /* OP_HASH160 */
              out[ 1] = 0x14;  /* push 20 bytes */
              memcpy(&out[2], &addrbin[1], 20);
              out[22] = 0x87;  /* OP_EQUAL */
              return rv;
          default:
              if (outsz < (rv = 25))
                  return rv;
              out[ 0] = 0x76;  /* OP_DUP */
              out[ 1] = 0xa9;  /* OP_HASH160 */
              out[ 2] = 0x14;  /* push 20 bytes */
              memcpy(&out[3], &addrbin[1], 20);
              out[23] = 0x88;  /* OP_EQUALVERIFY */
              out[24] = 0xac;  /* OP_CHECKSIG */
              return rv;
      }
  }

  int varint_encode(unsigned char *p, uint64_t n)
  {
      int i;
      if (n < 0xfd) {
          p[0] = (uint8_t) n;
          return 1;
      }
      if (n <= 0xffff) {
          p[0] = 0xfd;
          p[1] = n & 0xff;
          p[2] = (uint8_t) (n >> 8);
          return 3;
      }
      if (n <= 0xffffffff) {
          p[0] = 0xfe;
          for (i = 1; i < 5; i++) {
              p[i] = n & 0xff;
              n >>= 8;
          }
          return 5;
      }
      p[0] = 0xff;
      for (i = 1; i < 9; i++) {
          p[i] = n & 0xff;
          n >>= 8;
      }
      return 9;
  }


  void bin2hex(char *s, const unsigned char *p, size_t len)
  {
      for (size_t i = 0; i < len; i++)
          sprintf(s + (i * 2), "%02x", (unsigned int) p[i]);
  }

  char *abin2hex(const unsigned char *p, size_t len)
  {
      char *s = (char*) malloc((len * 2) + 1);
      if (!s)
          return NULL;
      bin2hex(s, p, len);
      return s;
  }

  bool hex2bin(unsigned char *p, const char *hexstr, size_t len)
  {
      char hex_byte[3];
      char *ep;

      hex_byte[2] = '\0';

      while (*hexstr && len) {
          if (!hexstr[1]) {
              //applog(LOG_ERR, "hex2bin str truncated");
              return false;
          }
          hex_byte[0] = hexstr[0];
          hex_byte[1] = hexstr[1];
          *p = (unsigned char) strtol(hex_byte, &ep, 16);
          if (*ep) {
              //applog(LOG_ERR, "hex2bin failed on '%s'", hex_byte);
              return false;
          }
          p++;
          hexstr += 2;
          len--;
      }

      return(!len) ? true : false;
/*  return (len == 0 && *hexstr == 0) ? true : false; */
  }

  bool fulltest(const uint32_t *hash, const uint32_t *target)
  {
    int i;
    bool rc = true;

    for (i = 7; i >= 0; i--) {
      if (hash[i] > target[i]) {
        rc = false;
        break;
      }
      if (hash[i] < target[i]) {
        rc = true;
        break;
      }
    }
    return rc;
  }




}


