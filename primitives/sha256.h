// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#ifndef BITCOIN_CRYPTO_SHA256_H
#define BITCOIN_CRYPTO_SHA256_H

#include <stdint.h>
#include <stdlib.h>

#if (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) || defined(__APPLE__)
    #include "common/portable_endian.h"
//#include "endian.h"
#endif

/**
 * @class CSHA256
 * @brief A hasher class for SHA-256.
 */
class CSHA256
{
private:
    uint32_t s[8];
    unsigned char buf[64];
    size_t bytes;

public:
    static const size_t OUTPUT_SIZE = 32;

    CSHA256();
    CSHA256& Write(const unsigned char* data, size_t len);
    void Finalize(unsigned char hash[OUTPUT_SIZE]);
    CSHA256& Reset();
};

#endif // BITCOIN_CRYPTO_SHA256_H
