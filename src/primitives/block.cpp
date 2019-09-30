// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "checkpoints_eb.h"
#include "arith_uint256.h"
#include <sstream>
#include <chainparams.h>

std::string CMTPHashData::toString()
{
    using std::begin;
    using std::end;

    std::stringstream ss;
    ss << "{";
    std::copy(begin(hashRootMTP), end(hashRootMTP), std::ostream_iterator<unsigned>(ss, ","));
    ss <<"};\n\n{";
    for (const auto& row : nBlockMTP)
    {
        ss << "{";
        std::copy(begin(row), end(row), std::ostream_iterator<unsigned>(ss, ","));
        ss << "},";
    }
    ss << "}\n\n{";
    for (const auto& arr : nProofMTP) {
        ss << "{";
        for (const auto &row : arr) {
            ss << "{";
            std::copy(begin(row), end(row), std::ostream_iterator<unsigned>(ss, ","));
            ss << "},";
        }
        ss << "},";
    }
    ss << "}\n";
    return  ss.str();
}

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetPoWHash() const
{
   return mtpHashValue;
}

unsigned int CBlock::GetStakeEntropyBit(int32_t height) const
{
    unsigned int nEntropyBit = 0;
    if (IsProtocolV04(nTime))
        nEntropyBit = UintToArith256(GetHash()).GetLow64() & 1llu;// last bit of block hash
    else if (height > -1 && height <= vEntropyBits_number_of_blocks)
        // old protocol for entropy bit pre v0.4; exctracted from precomputed table.
        nEntropyBit = (vEntropyBits[height >> 5] >> (height & 0x1f)) & 1;

    return nEntropyBit;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, nFlags=%i, nVersionMTP=%u, mtpHashValue=%s, mtpHashData=%b, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce, nFlags,
        nVersionMTP, mtpHashValue.ToString(),
        bool(mtpHashData),
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i]->ToString() << "\n";
    }
    return s.str();
}

int64_t GetBlockWeight(const CBlock& block)
{
    // This implements the weight = (stripped_size * 4) + witness_size formula,
    // using only serialization with and without witness data. As witness_size
    // is equal to total_size - stripped_size, this formula is identical to:
    // weight = (stripped_size * 3) + total_size.
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}


// Whether the given coinstake is subject to new v0.3 protocol
bool IsProtocolV03(unsigned int nTimeCoinStake)
{
    return (nTimeCoinStake >= 1363800000);  // 03/20/2013 @ 5:20pm (UTC)
}

// Whether the given block is subject to new v0.4 protocol
bool IsProtocolV04(unsigned int nTimeBlock)
{
    return (nTimeBlock >= 1449100800);      // 12/03/2015 @ 12:00am (UTC)
}

// Whether the given transaction is subject to new v0.5 protocol
bool IsProtocolV05(unsigned int nTimeTx, const CChainParams& chainparams)
{
    return (nTimeTx >= (chainparams.GenesisBlock().nTime + chainparams.GetConsensus().nStakeMinAge));
}
