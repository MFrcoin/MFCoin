// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include "primitives/transaction.h"
#include "serialize.h"
#include "uint256.h"
#include "crypto/MerkleTreeProof/mtp.h"

class CChainParams;
class CAuxPow;
template<typename Stream> void SerReadWrite(Stream& s, std::shared_ptr<CAuxPow>& pobj, CSerActionSerialize ser_action);
template<typename Stream> void SerReadWrite(Stream& s, std::shared_ptr<CAuxPow>& pobj, CSerActionUnserialize ser_action);

// merged mining stuff
static const int AUXPOW_CHAIN_ID = 666;

enum
{
    // primary version
    BLOCK_VERSION_DEFAULT        = (1 << 0),

    // modifiers
    BLOCK_VERSION_AUXPOW         = (1 << 8),

    // bits allocated for chain ID
    BLOCK_VERSION_CHAIN_START    = (1 << 16),
    BLOCK_VERSION_CHAIN_END      = (1 << 30),
};

// ppcoin enum
enum
{
    BLOCK_PROOF_OF_STAKE = (1 << 0), // is proof-of-stake block
    BLOCK_STAKE_ENTROPY  = (1 << 1), // entropy bit for stake modifier
    BLOCK_STAKE_MODIFIER = (1 << 2), // regenerated stake modifier
};

// MTP
class CMTPHashData {
public:
   uint8_t hashRootMTP[16]; // 16 is 128 bit of blake2b
   uint64_t nBlockMTP[mtp::MTP_L*2][128]; // 128 is ARGON2_QWORDS_IN_BLOCK
   std::deque<std::vector<uint8_t>> nProofMTP[mtp::MTP_L*3];

   CMTPHashData() {
      memset(nBlockMTP, 0, sizeof(nBlockMTP));
   }

   ADD_SERIALIZE_METHODS;

   /**
    * Custom serialization scheme is in place because of speed reasons
    */

   // Function for write/getting size
   template <typename Stream, typename Operation, typename = typename std::enable_if<!std::is_base_of<CSerActionUnserialize, Operation>::value>::type>
   inline void SerializationOp(Stream &s, Operation ser_action) {
      READWRITE(hashRootMTP);
      READWRITE(nBlockMTP);
      for (int i = 0; i < mtp::MTP_L*3; i++) {
         assert(nProofMTP[i].size() < 256);
         uint8_t numberOfProofBlocks = (uint8_t)nProofMTP[i].size();
         READWRITE(numberOfProofBlocks);
         for (const std::vector<uint8_t> &mtpData: nProofMTP[i]) {
            // data size should be 16 for each block
            assert(mtpData.size() == 16);
            s.write((const char *)mtpData.data(), 16);
         }
      }
   }

   // Function for reading
   template <typename Stream>
   inline void SerializationOp(Stream &s, CSerActionUnserialize ser_action) {
      READWRITE(hashRootMTP);
      READWRITE(nBlockMTP);
      for (int i = 0; i < mtp::MTP_L*3; i++) {
         uint8_t numberOfProofBlocks;
         READWRITE(numberOfProofBlocks);
         for (uint8_t j=0; j<numberOfProofBlocks; j++) {
            std::vector<uint8_t> mtpData(16, 0);
            s.read((char *)mtpData.data(), 16);
            nProofMTP[i].emplace_back(std::move(mtpData));
         }
      }
   }

   std::string toString();
};

template<typename Stream> void SerReadWrite(Stream& s, std::shared_ptr<CMTPHashData>& pobj, CSerActionSerialize ser_action)
{
    if (pobj && !(s.GetType() & SER_GETHASH))
        ::Serialize(s, *pobj);
}

template<typename Stream> void SerReadWrite(Stream& s, std::shared_ptr<CMTPHashData>& pobj, CSerActionUnserialize ser_action)
{
   pobj.reset(new CMTPHashData());
   ::Unserialize(s, *pobj);
}

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    static const int32_t NORMAL_SERIALIZE_SIZE=116;
    static const int32_t CURRENT_VERSION=8;
    static const int32_t CURRENT_MTP_VERSION=0x1000;
    int32_t nVersion;     // mfcoin: it might contain merged mining information in higher bits. Use GetBlockVersion() to ignore it.
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
    // MTP
    int32_t nVersionMTP = 0x1000;
    uint256 mtpHashValue;
    // Merged mining
    std::shared_ptr<CAuxPow> auxpow;

    // Store this only when absolutely needed for verification
    std::shared_ptr<CMTPHashData> mtpHashData;

    // mfcoin: copy from CBlockIndex.nFlags from other clients. We need this information because we are using headers-first syncronization.
    int32_t nFlags;


    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    class CSerializeBlockHeader {};
    class CReadBlockHeader : public CSerActionUnserialize, public CSerializeBlockHeader {};
    class CWriteBlockHeader : public CSerActionSerialize, public CSerializeBlockHeader {};

    template <typename Stream, typename Operation, typename = typename std::enable_if<!std::is_base_of<CSerializeBlockHeader,Operation>::value>::type>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        READWRITE(nVersionMTP);
        READWRITE(mtpHashValue);

        // mfcoin: do not serialize these fields when computing hash
        if (!(s.GetType() & SER_GETHASH)) {
            if (this->nVersion & BLOCK_VERSION_AUXPOW)
                READWRITE(auxpow);
            else
                auxpow.reset();
        }

        if (!(nFlags & BLOCK_PROOF_OF_STAKE) && mtpHashValue != uint256()) {
            //READWRITE(mtpHashData);
            if (ser_action.ForRead()) {
                mtpHashData = std::make_shared<CMTPHashData>();
                READWRITE(*mtpHashData);
            } else {
                if (mtpHashData && !(s.GetType() & SER_GETHASH))
                    READWRITE(*mtpHashData);
            }
        }

        if (!(s.GetType() & SER_GETHASH)) {
            if (s.GetType() & SER_POSMARKER)
                READWRITE(nFlags);
        }
    }

    template <typename Stream>
    inline void SerializationOp(Stream& s, CReadBlockHeader ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        READWRITE(nVersionMTP);
        READWRITE(mtpHashValue);

        // mfcoin: do not serialize these fields when computing hash
        if (!(s.GetType() & SER_GETHASH)) {
            if (this->nVersion & BLOCK_VERSION_AUXPOW)
                READWRITE(auxpow);
            else
                auxpow.reset();

            if (s.GetType() & SER_POSMARKER)
                READWRITE(nFlags);
        }
    }

    void SetNull()
    {
        SetBlockVersion(CBlockHeader::CURRENT_VERSION);
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        nFlags = 0;
        mtpHashData = nullptr;
        mtpHashValue.SetNull();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;
    uint256 GetPoWHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    int GetChainID() const
    {
        return nVersion / BLOCK_VERSION_CHAIN_START;
    }

    void SetAuxPow(CAuxPow* pow);

    int32_t GetBlockVersion() const
    {
        return nVersion & 0xff;
    }
    void SetBlockVersion(int32_t nBlockVersion)
    {
        nVersion = nBlockVersion | (AUXPOW_CHAIN_ID * BLOCK_VERSION_CHAIN_START);
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // ppcoin: block signature - signed by coin base txout[0]'s owner
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

/*    template <typename Stream>
    inline void SerializationOp(Stream &s, CReadBlockHeader ser_action) {
        CBlockHeader::SerializationOp(s, ser_action);
        //READWRITE(vtx);
    }*/


    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CBlockHeader*)this);
        READWRITE(vtx);
        READWRITE(vchBlockSig);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
        vchBlockSig.clear();
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        block.nVersionMTP    = nVersionMTP;
        //block.mtpHashValue   = mtpHashValue;
        block.auxpow         = auxpow;
        block.mtpHashData    = mtpHashData;
        block.nFlags         = nFlags;
        return block;
    }

    // ppcoin: entropy bit for stake modifier if chosen by modifier
    // if height is specified a special table with precomputed bits is used
    unsigned int GetStakeEntropyBit(int32_t height) const;

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    bool IsProofOfStake() const
    {
        return (vtx.size() > 1 && vtx[1]->IsCoinStake());
    }

    bool IsProofOfWork() const
    {
        return !IsProofOfStake();
    }

    std::pair<COutPoint, unsigned int> GetProofOfStake() const
    {
        return IsProofOfStake()? std::make_pair(vtx[1]->vin[0].prevout, vtx[1]->nTime) : std::make_pair(COutPoint(), (unsigned int)0);
    }

    // ppcoin: get max transaction timestamp
    int64_t GetMaxTransactionTime() const
    {
        int64_t maxTransactionTime = 0;
        for (const auto& tx : vtx)
            maxTransactionTime = std::max(maxTransactionTime, (int64_t)tx->nTime);
        return maxTransactionTime;
    }


    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    CBlockLocator(const std::vector<uint256>& vHaveIn)
    {
        vHave = vHaveIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

/** Compute the consensus-critical block weight (see BIP 141). */
int64_t GetBlockWeight(const CBlock& tx);


// Whether the given coinstake is subject to specified protocol
bool IsProtocolV03(unsigned int nTimeCoinStake);
bool IsProtocolV04(unsigned int nTimeBlock);
bool IsProtocolV05(unsigned int nTimeTx, const CChainParams& chainparams);

#endif // BITCOIN_PRIMITIVES_BLOCK_H
