/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file EthashAux.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <condition_variable>
#include <libethash/ethash.h>
#include <libdevcore/Log.h>
#include <libdevcore/Worker.h>
#include "BlockHeader.h"

namespace dev
{
namespace eth
{

struct Solution
{
	uint64_t nonce;
	h256 mixHash;
	h256 headerHash;
	h256 seedHash;
	h256 boundary;
};

struct Result
{
	h256 value;
	h256 mixHash;
};

class EthashAux
{
public:
	struct LightAllocation
	{
		LightAllocation(h256 const& _seedHash);
		~LightAllocation();
		bytesConstRef data() const;
		Result compute(h256 const& _headerHash, uint64_t _nonce) const;
		ethash_light_t light;
		uint64_t size;
	};

	using LightType = std::shared_ptr<LightAllocation>;

	static h256 seedHash(unsigned _number);
	static uint64_t number(h256 const& _seedHash);

	static LightType light(h256 const& _seedHash);

	static Result eval(h256 const& _seedHash, h256 const& _headerHash, uint64_t  _nonce) noexcept;

private:
	EthashAux() = default;
	static EthashAux& get();

	Mutex x_lights;
	std::unordered_map<h256, LightType> m_lights;

	Mutex x_epochs;
	std::unordered_map<h256, unsigned> m_epochs;
	h256s m_seedHashes;
};

/*
// TODO: unit tests / validation
::std::string get_seedhash(uint64_t const block_number)
{
	::std::string s(epoch0_seedhash, size_epoch0_seedhash);
	for (size_t i = 0; i < (block_number / constants::EPOCH_LENGTH); i++)
	{
		s = sha3_256_t::serialize(sha3_256(s));
	}
	return s;
}

	/** \brief epoch0_seedhash is is the seed hash for the genesis block and first epoch of the DAG.
	*			All seed hashes for subsequent epochs will be generated from this seedhash.
	*
	*	The epoch0_seedhash should be set to a randomized set of 32 bytes for a given crypto currency.
	*	This represents a keccak-256 hash that will be used as input for building the DAG/cache.
	* /
	static constexpr char epoch0_seedhash[] = "\xa8\x49\x4b\xb2\x89\x5b\xd7\xed\x18\xbb\x39\xb7\xb2\x8a\xf5\x1d\xec\x51\xf7\xca\xd3\x30\xc1\x68\xf1\xbd\x1c\x90\xe7\x61\x4c\x32";
	static constexpr uint8_t size_epoch0_seedhash = sizeof(epoch0_seedhash) - 1;
	static_assert(size_epoch0_seedhash == 32, "Invalid seedhash");

*/


struct WorkPackage
{
	WorkPackage() = default;
	explicit WorkPackage(BlockHeader const& _bh) :
		boundary(_bh.boundary()),
		header(_bh.hashWithout()),
		seed(EthashAux::seedHash(static_cast<unsigned>(_bh.number())))
	{ }
	void reset() { header = h256(); }
	explicit operator bool() const { return header != h256(); }

	h256 boundary;
	h256 header;	///< When h256() means "pause until notified a new work package is available".
	h256 seed;

	uint64_t startNonce = 0;
	int exSizeBits = -1;
};

}
}
