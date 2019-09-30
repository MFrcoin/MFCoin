// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHECKPOINTS_H
#define BITCOIN_CHECKPOINTS_H

#include "uint256.h"
#include "serialize.h"

#include <map>

class CBlockIndex;
class CNode;
class CSyncCheckpoint;
struct CCheckpointData;

extern std::string strMasterPubKey;

/**
 * Block-chain checkpoints are compiled-in sanity checks.
 * They are updated every release or three.
 */
namespace Checkpoints
{

//! Returns last CBlockIndex* in mapBlockIndex that is a checkpoint
CBlockIndex* GetLastCheckpoint(const CCheckpointData& data);
bool ValidateBlockHeader(const CCheckpointData& data, int nHeight, const uint256& hash);
} //namespace Checkpoints

namespace CheckpointsSync
{

    extern uint256 hashSyncCheckpoint;
    extern uint256 hashPendingCheckpoint;
    extern CSyncCheckpoint checkpointMessage;
    extern uint256 hashInvalidCheckpoint;

    bool WriteSyncCheckpoint(const uint256& hashCheckpoint);
    bool AcceptPendingSyncCheckpoint();
    uint256 AutoSelectSyncCheckpoint();
    bool CheckSync(const CBlockIndex* pindexNew);
    bool ResetSyncCheckpoint();
    bool SetCheckpointPrivKey(std::string strPrivKey);
    bool SendSyncCheckpoint(uint256 hashCheckpoint);
    bool IsSyncCheckpointTooOld(unsigned int nSeconds);

}  //namespace CheckpointsSync

// ppcoin: synchronized checkpoint
class CUnsignedSyncCheckpoint
{
public:
    int nVersion;
    uint256 hashCheckpoint;      // checkpoint block

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashCheckpoint);
    }

    void SetNull()
    {
        nVersion = 1;
        hashCheckpoint = uint256();
    }

    std::string ToString() const;

    void print() const;
};

class CSyncCheckpoint : public CUnsignedSyncCheckpoint
{
public:
    static std::string strMasterPrivKey;

    std::vector<unsigned char> vchMsg;
    std::vector<unsigned char> vchSig;

    CSyncCheckpoint()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vchMsg);
        READWRITE(vchSig);
    }

    void SetNull()
    {
        CUnsignedSyncCheckpoint::SetNull();
        vchMsg.clear();
        vchSig.clear();
    }

    bool IsNull() const
    {
        return (hashCheckpoint == uint256());
    }

    uint256 GetHash() const;

    bool RelayTo(CNode* pnode) const;

    bool CheckSignature();
    bool ProcessSyncCheckpoint();
};

#endif // BITCOIN_CHECKPOINTS_H
