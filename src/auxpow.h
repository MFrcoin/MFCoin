// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_AUXPOW_H
#define BITCOIN_AUXPOW_H

#include "primitives/block.h"
#include "primitives/transaction.h"
#include "streams.h"
#include "hash.h"

//unsigned char pchMergedMiningHeader[] = { 0xfa, 0xbe, 'm', 'm' };

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTxSmall
{
public:
    CTransactionRef tx;
    uint256 hashBlock;
    std::vector<uint256> vMerkleBranch;
    int nIndex;

    CMerkleTxSmall()
    {
        SetTx(MakeTransactionRef());
        Init();
    }

    CMerkleTxSmall(CTransactionRef arg)
    {
        SetTx(std::move(arg));
        Init();
    }

    /** Helper conversion operator to allow passing CMerkleTx where CTransaction is expected.
     *  TODO: adapt callers and remove this operator. */
    operator const CTransaction&() const { return *tx; }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
    }

    void SetTx(CTransactionRef arg)
    {
        tx = std::move(arg);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(tx);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }
};

class CAuxPow : public CMerkleTxSmall
{
public:
    CAuxPow() : CMerkleTxSmall() {}

    // Merkle branch with root vchAux
    // root must be present inside the coinbase
    std::vector<uint256> vChainMerkleBranch;
    // Index of chain in chains merkle tree
    unsigned int nChainIndex;
    CBlockHeader parentBlockHeader;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CMerkleTxSmall*)this);
        READWRITE(vChainMerkleBranch);
        READWRITE(nChainIndex);
        // Always serialize the saved parent block as header so that the size of CAuxPow
        // is consistent.
        READWRITE(parentBlockHeader);
    }

    uint256 GetParentBlockHash()
    {
       return parentBlockHeader.GetHash();
    }
};

template<typename Stream> void SerReadWrite(Stream& s, std::shared_ptr<CAuxPow>& pobj, CSerActionSerialize ser_action)
{
    ::Serialize(s, *pobj);
}

template<typename Stream> void SerReadWrite(Stream& s, std::shared_ptr<CAuxPow>& pobj, CSerActionUnserialize ser_action)
{
    pobj.reset(new CAuxPow());
    ::Unserialize(s, *pobj);
}

template void SerReadWrite<CAutoFile>(CAutoFile& s, std::shared_ptr<CAuxPow>& pobj, CSerActionSerialize ser_action);
template void SerReadWrite<CVectorWriter>(CVectorWriter& s, std::shared_ptr<CAuxPow>& pobj, CSerActionSerialize ser_action);
template void SerReadWrite<CHashWriter>(CHashWriter& s, std::shared_ptr<CAuxPow>& pobj, CSerActionSerialize ser_action);
template void SerReadWrite<CDataStream>(CDataStream& s, std::shared_ptr<CAuxPow>& pobj, CSerActionSerialize ser_action);
template void SerReadWrite<CSizeComputer>(CSizeComputer& s, std::shared_ptr<CAuxPow>& pobj, CSerActionSerialize ser_action);

template void SerReadWrite<CAutoFile>(CAutoFile& s, std::shared_ptr<CAuxPow>& pobj, CSerActionUnserialize ser_action);
template void SerReadWrite<CBufferedFile>(CBufferedFile& s, std::shared_ptr<CAuxPow>& pobj, CSerActionUnserialize ser_action);
template void SerReadWrite<CDataStream>(CDataStream& s, std::shared_ptr<CAuxPow>& pobj, CSerActionUnserialize ser_action);

#endif
