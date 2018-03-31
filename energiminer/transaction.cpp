#include "transaction.h"
#include "energiminer/common/utilstrencodings.h"

namespace energi
{
    // TODO: This only supports P2PKH addresses. This should support all address types. (issue #14)
    void GetScriptForDestination(const CKeyID& keyID, unsigned char* out)
    {
        out[ 0] = 0x76;  /* OP_DUP */
        out[ 1] = 0xa9;  /* OP_HASH160 */
        out[ 2] = 0x14;  /* push 20 bytes */
        std::copy(keyID.begin(), keyID.end(), &out[3]);
        out[23] = 0x88;  /* OP_EQUALVERIFY */
        out[24] = 0xac;  /* OP_CHECKSIG */
    }

    txo_script::txo_script(std::string const & scriptHex)
    : vbyte(scriptHex.size() / 2, 0)
    {
        if (!IsHex(scriptHex)) {
            throw WorkException("Cannot decode script");
        }
        auto data = ParseHex(scriptHex);
        swap(data);
    }

    txo_script::txo_script(CBitcoinAddress const & address)
    : vbyte(25, 0)
    {
        CKeyID keyID;
        if (!address.GetKeyID(keyID))
        {
            throw WorkException("Could not get KeyID for address");
        }

        GetScriptForDestination(keyID, data());
    }

    blocktemplate_payee::blocktemplate_payee()
    : payee()
    , script()
    , amount(0)
    {

    }

    blocktemplate_payee::blocktemplate_payee(Json::Value json)
    : payee()
    , script()
    , amount(0)
    {
        // ensure well formed JSON
        if (json.isMember("payee") && json.isMember("script") && json.isMember("amount"))
        {
            payee = json["payee"].asString();
            script = txo_script(json["script"].asString());
            amount = json["amount"].asUInt();
        }
    }

    coinbase_tx::coinbase_tx(CAmount value, std::string const & miner_address)
    : value(value)
    , miner_script(CBitcoinAddress(miner_address))
    {

    }

    void coinbase_tx::add(blocktemplate_payee && payee)
    {
        cb_outputs.push_back(payee);
    }

    vbyte coinbase_tx::data()
    {
        vbyte ret;

        CAmount left_for_miner = value;

        for (auto const & i: cb_outputs)
        {
            left_for_miner -= i.amount;
            // add the amount owed to this payee
            ret.insert(ret.end(), reinterpret_cast<uint8_t const *>(&i.amount), reinterpret_cast<uint8_t const *>(&i.amount) + 8);
            // add the size of the payee script
            ret.push_back(static_cast<uint8_t>(i.script.size()));
            // add the payee script
            ret.insert(ret.end(), i.script.begin(), i.script.end());
        }

        // implicitly add the miner's output
        ret.insert(ret.end(), reinterpret_cast<uint8_t const *>(&left_for_miner), reinterpret_cast<uint8_t const *>(&left_for_miner) + 8);
        ret.push_back(static_cast<uint8_t>(miner_script.size()));
        ret.insert(ret.end(), miner_script.begin(), miner_script.end());

        return ret;
    }
}
