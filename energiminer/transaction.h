#ifndef ENERGIMINER_TRANSACTION_H
#define ENERGIMINER_TRANSACTION_H

#include <stdint.h>
#include <string>
#include <json/json.h>

#include "energiminer/common.h"
#include "primitives/base58.h"

namespace energi
{
	typedef int64_t CAmount;

	// TODO: This only supports P2PKH addresses. This should support all address types. (issue #14)
	void GetScriptForDestination(const CKeyID& keyID, unsigned char* out);

	struct txo_script : public vbyte
	{
	    txo_script(std::string const & scriptHex);
	    txo_script(CBitcoinAddress const & address);
	    txo_script() = default;
	    txo_script(txo_script const &) = default;
	    txo_script(txo_script &&) = default;
	    txo_script & operator=(txo_script const &) = default;
	    txo_script & operator=(txo_script &&) = default;
	    ~txo_script() = default;
	};

	// represents a payee from getblocktemplate, i.e. Energi Backbone or a Masternode
	struct blocktemplate_payee
	{
	    // default
	    blocktemplate_payee();
	    blocktemplate_payee(Json::Value json);
	    blocktemplate_payee(blocktemplate_payee const &) = default;
	    blocktemplate_payee& operator=(blocktemplate_payee const &) = default;
	    blocktemplate_payee(blocktemplate_payee &&) = default;
	    blocktemplate_payee& operator=(blocktemplate_payee &&) = default;
	    ~blocktemplate_payee() = default;

	    std::string payee;
	    txo_script script;
	    CAmount amount;
	};

	// represents the entire coinbase transaction, i.e. set of all payees
	struct coinbase_tx
	{
	    coinbase_tx(CAmount value, std::string const & miner_address);
	    void add(blocktemplate_payee && payee);
	    vbyte data();

	    std::vector<blocktemplate_payee> cb_outputs;
	    CAmount value;
	    txo_script miner_script;
	};
}

#endif
