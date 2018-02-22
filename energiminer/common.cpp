/*
 * common.cpp
 *
 *  Created on: Dec 16, 2017
 *      Author: ranjeet
 */

#include "common.h"

namespace energi
{
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


