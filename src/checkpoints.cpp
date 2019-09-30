// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"

#include "chain.h"
#include "chainparams.h"
#include "txdb.h"
#include "validation.h"
#include "uint256.h"
#include "util.h"
#include "netmessagemaker.h"
#include "key.h"

#include <stdint.h>

#include <boost/foreach.hpp>


/** Synchronized checkpoint public key */
std::string strMasterPubKey = "";

namespace Checkpoints {
    bool ValidateBlockHeader(const CCheckpointData& data, int nHeight, const uint256& hash) {
        const MapCheckpoints& checkpoints = data.mapCheckpoints;
        MapCheckpoints::const_iterator cpit = checkpoints.upper_bound(nHeight - 1);
        if (cpit == checkpoints.end())
            return true;
        return (cpit->first == nHeight)?
            hash == cpit->second
            :
            mapBlockIndex.find(cpit->second) == mapBlockIndex.end();
    }
} // namespace Checkpoints



namespace CheckpointsSync {

// ppcoin: synchronized checkpoint (centrally broadcasted)
uint256 hashSyncCheckpoint = uint256();
uint256 hashPendingCheckpoint = uint256();
CSyncCheckpoint checkpointMessage;
CSyncCheckpoint checkpointMessagePending;
uint256 hashInvalidCheckpoint = uint256();

// ppcoin: only descendant of current sync-checkpoint is allowed
bool ValidateSyncCheckpoint(uint256 hashCheckpoint)
{
    if (!mapBlockIndex.count(hashSyncCheckpoint))
        return error("ValidateSyncCheckpoint: block index missing for current sync-checkpoint %s", hashSyncCheckpoint.ToString());
    if (!mapBlockIndex.count(hashCheckpoint))
        return error("ValidateSyncCheckpoint: block index missing for received sync-checkpoint %s", hashCheckpoint.ToString());

    CBlockIndex* pindexSyncCheckpoint = mapBlockIndex[hashSyncCheckpoint];
    CBlockIndex* pindexCheckpointRecv = mapBlockIndex[hashCheckpoint];

    if (pindexCheckpointRecv->nHeight <= pindexSyncCheckpoint->nHeight)
    {
        // Received an older checkpoint, trace back from current checkpoint
        // to the same height of the received checkpoint to verify
        // that current checkpoint should be a descendant block
        if (!chainActive.Contains(pindexCheckpointRecv))
        {
            hashInvalidCheckpoint = hashCheckpoint;
            return error("ValidateSyncCheckpoint: new sync-checkpoint %s is conflicting with current sync-checkpoint %s", hashCheckpoint.ToString(), hashSyncCheckpoint.ToString());
        }
        return false; // ignore older checkpoint
    }

    // Received checkpoint should be a descendant block of the current
    // checkpoint. Trace back to the same height of current checkpoint
    // to verify.
    CBlockIndex* pindex = pindexCheckpointRecv;
    while (pindex->nHeight > pindexSyncCheckpoint->nHeight)
        if (!(pindex = pindex->pprev))
            return error("ValidateSyncCheckpoint: pprev2 null - block index structure failure");
    if (pindex->GetBlockHash() != hashSyncCheckpoint)
    {
        hashInvalidCheckpoint = hashCheckpoint;
        return error("ValidateSyncCheckpoint: new sync-checkpoint %s is not a descendant of current sync-checkpoint %s", hashCheckpoint.ToString(), hashSyncCheckpoint.ToString());
    }
    return true;
}

bool WriteSyncCheckpoint(const uint256& hashCheckpoint)
{
    if (!pblocktree->WriteSyncCheckpoint(hashCheckpoint))
    {
        return error("WriteSyncCheckpoint(): failed to write to txdb sync checkpoint %s", hashCheckpoint.ToString());
    }
    FlushStateToDisk();
    hashSyncCheckpoint = hashCheckpoint;
    return true;
}

bool AcceptPendingSyncCheckpoint()
{
    if (strMasterPubKey == "") return false;  // no public key == no checkpoints

    LOCK(cs_main);
    bool havePendingCheckpoint = hashPendingCheckpoint != uint256() && mapBlockIndex.count(hashPendingCheckpoint);
    if (!havePendingCheckpoint)
        return false;

    if (!ValidateSyncCheckpoint(hashPendingCheckpoint))
    {
        hashPendingCheckpoint = uint256();
        checkpointMessagePending.SetNull();
        return false;
    }

    if (!chainActive.Contains(mapBlockIndex[hashPendingCheckpoint]))
        return false;

    // mfcoin: checkpoint needs to be a block with 32 confirmation
    if (mapBlockIndex[hashPendingCheckpoint]->nHeight > chainActive.Height() - 32)
        return false;

    if (!WriteSyncCheckpoint(hashPendingCheckpoint))
        return error("AcceptPendingSyncCheckpoint(): failed to write sync checkpoint %s", hashPendingCheckpoint.ToString());
    hashPendingCheckpoint = uint256();
    checkpointMessage = checkpointMessagePending;
    checkpointMessagePending.SetNull();
    LogPrintf("AcceptPendingSyncCheckpoint : sync-checkpoint at %s\n", hashSyncCheckpoint.ToString());
    // relay the checkpoint
    if (g_connman && !checkpointMessage.IsNull())
        g_connman->ForEachNode([](CNode* pnode) {
            checkpointMessage.RelayTo(pnode);
        });
    return true;
}


// Automatically select a suitable sync-checkpoint
uint256 AutoSelectSyncCheckpoint()
{
    static int32_t  s_depth = -1;
    static uint32_t s_slots, s_node_no;
    if (s_depth < 0)
    {
        s_depth   = GetArg("-checkpointdepth", 174 * 5); // default is 5 days backward deep
        s_slots   = GetArg("-checkpointslots", 1); // quantity of check slots, def=1
        s_node_no = GetArg("-checkpointnode", 0); // Number of current slot,  def=0
    }

    const CBlockIndex *pindex = chainActive.Tip();

    // Get hash of current block stamp in specific depth
    for (int32_t i = 0; i < s_depth; i++)
        if((pindex = pindex->pprev) == NULL)
            return uint256();

    const CBlockIndex *rc = pindex;

    // Get H-selector from checkpointed stable area +32+6 deeper
    for (int i = 0; i < 38; i++)
        if((pindex = pindex->pprev) == NULL)
            return uint256();

    uint256 h = pindex->GetBlockHash();

    // Preserve analyze current printer node from public block hash
    uint256 hx = Hash(CSyncCheckpoint::strMasterPrivKey.begin(), CSyncCheckpoint::strMasterPrivKey.end(), h.GetDataPtr(), h.GetDataPtr() + 256 / 32);

    return (hx.GetDataPtr()[0] % s_slots == s_node_no) ? rc->GetBlockHash() : uint256();
}

// Check against synchronized checkpoint
bool CheckSync(const CBlockIndex* pindexNew)
{
    if (strMasterPubKey.empty()) return true;     // no public key == no checkpoints
    LOCK(cs_main);
    assert(pindexNew != NULL);
    int nHeight = pindexNew->nHeight;
    if (nHeight == 0) return true;                // genesis cannot be checked against previous block
    if (chainActive.Height() == 0) return true;   // skip checks during reindex

    // sync-checkpoint should always be accepted block
    assert(mapBlockIndex.count(hashSyncCheckpoint));
    const CBlockIndex* pindexSync = mapBlockIndex[hashSyncCheckpoint];
    assert(chainActive.Contains(pindexSync));

    if (nHeight > pindexSync->nHeight)
    {
        // trace back to first block in our chainActive
        const CBlockIndex* pindex = pindexNew;
        bool rc;
        while (!(rc = chainActive.Contains(pindex)) && pindex->nHeight > pindexSync->nHeight)
            if (!(pindex = pindex->pprev))
                return error("CheckSync: pprev null - block index structure failure");

        // at this point we could have:
        // 1. found block in our blockchain
        // 2. reached pindexSync->nHeight without finding it
        return rc;
    }
    if (nHeight == pindexSync->nHeight) // same height with sync-checkpoint
        return pindexNew->GetBlockHash() == hashSyncCheckpoint;

    // lower height than sync-checkpoint
    return chainActive.Contains(pindexNew);
}

// ppcoin: reset synchronized checkpoint to last hardened checkpoint
bool ResetSyncCheckpoint()
{
    LOCK(cs_main);
    if (!WriteSyncCheckpoint(Params().GetConsensus().hashGenesisBlock))
        return error("ResetSyncCheckpoint: failed to write sync checkpoint %s", Params().GetConsensus().hashGenesisBlock.ToString());
    LogPrintf("ResetSyncCheckpoint: sync-checkpoint reset to %s\n", hashSyncCheckpoint.ToString());
    return true;
}

bool SetCheckpointPrivKey(std::string strPrivKey)
{
    // Test signing a sync-checkpoint with genesis block
    CSyncCheckpoint checkpoint;
    checkpoint.hashCheckpoint = Params().GetConsensus().hashGenesisBlock;
    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedSyncCheckpoint)checkpoint;
    checkpoint.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

    std::vector<unsigned char> vchPrivKey = ParseHex(strPrivKey);
    CKey key;
    if(!key.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end()), false))
        LogPrintf("SetCheckpointPrivKey(): failed at key.SetPrivKey\n"); // if key is not correct openssl may crash
    if (!key.Sign(Hash(checkpoint.vchMsg.begin(), checkpoint.vchMsg.end()), checkpoint.vchSig))
        return false;

    // Test signing successful, proceed
    CSyncCheckpoint::strMasterPrivKey = strPrivKey;
    return true;
}

bool SendSyncCheckpoint(uint256 hashCheckpoint)
{
    if (!g_connman)
        return true;  // P2P disabled - nothing to do

    if (g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0)
        return true; // no one to send to

    if (hashCheckpoint == uint256())
        return true; // don't send dummy checkpoint

    CSyncCheckpoint checkpoint;
    checkpoint.hashCheckpoint = hashCheckpoint;
    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedSyncCheckpoint)checkpoint;
    checkpoint.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());

    if (CSyncCheckpoint::strMasterPrivKey.empty())
        return error("SendSyncCheckpoint: Checkpoint master key unavailable.");
    std::vector<unsigned char> vchPrivKey = ParseHex(CSyncCheckpoint::strMasterPrivKey);
    CKey key;
    key.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end()), false); // if key is not correct openssl may crash
    if (!key.Sign(Hash(checkpoint.vchMsg.begin(), checkpoint.vchMsg.end()), checkpoint.vchSig))
        return error("SendSyncCheckpoint: Unable to sign checkpoint, check private key?");

    if(!checkpoint.ProcessSyncCheckpoint())
    {
        LogPrintf("WARNING: SendSyncCheckpoint: Failed to process checkpoint.\n");
        return false;
    }

    // Relay checkpoint
    g_connman->ForEachNode([&checkpoint](CNode* pnode) {
        checkpoint.RelayTo(pnode);
    });
    return true;
}

// Is the sync-checkpoint too old?
bool IsSyncCheckpointTooOld(unsigned int nSeconds)
{
    if (strMasterPubKey == "") return false;  // no public key == no checkpoints

    if (Params().NetworkIDString() == "test")  // requested by Olegh
        return false;

    LOCK(cs_main);
    // sync-checkpoint should always be accepted block
    assert(mapBlockIndex.count(hashSyncCheckpoint));
    const CBlockIndex* pindexSync = mapBlockIndex[hashSyncCheckpoint];
    //assert(chainActive.Contains(pindexSync));  //disabled for reconsiderblock function

    return (pindexSync->GetBlockTime() + nSeconds < GetAdjustedTime());
}

}  // namespace CheckpointsSync




// mfcoin: sync-checkpoint master key
std::string CSyncCheckpoint::strMasterPrivKey = "";

// ppcoin: verify signature of sync-checkpoint message
bool CSyncCheckpoint::CheckSignature()
{
    CPubKey key(ParseHex(strMasterPubKey));
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        return error("CSyncCheckpoint::CheckSignature() : verify signature failed");

    // Now unserialize the data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedSyncCheckpoint*)this;
    return true;
}

// ppcoin: process synchronized checkpoint
bool CSyncCheckpoint::ProcessSyncCheckpoint()
{
    if (strMasterPubKey == "") return false;  // no public key == no checkpoints

    if (!CheckSignature())
        return false;

    LOCK(cs_main);
    if (!mapBlockIndex.count(hashCheckpoint))
    {
        LogPrintf("Missing headers for received sync-checkpoint %s\n", hashCheckpoint.ToString());
        return false;
    }

    if (!CheckpointsSync::ValidateSyncCheckpoint(hashCheckpoint))
        return false;

    bool pass = chainActive.Contains(mapBlockIndex[hashCheckpoint]) &&
                mapBlockIndex[hashCheckpoint]->nHeight <= chainActive.Height() - 2;
    if (!pass)
    {
        // We haven't received the checkpoint chain, keep the checkpoint as pending
        CheckpointsSync::hashPendingCheckpoint = hashCheckpoint;
        CheckpointsSync::checkpointMessagePending = *this;
        LogPrintf("ProcessSyncCheckpoint: pending for sync-checkpoint %s\n", hashCheckpoint.ToString());
        return false;
    }

    if (!CheckpointsSync::WriteSyncCheckpoint(hashCheckpoint))
        return error("ProcessSyncCheckpoint(): failed to write sync checkpoint %s", hashCheckpoint.ToString());
    CheckpointsSync::checkpointMessage = *this;
    CheckpointsSync::hashPendingCheckpoint = uint256();
    CheckpointsSync::checkpointMessagePending.SetNull();
    LogPrintf("ProcessSyncCheckpoint: sync-checkpoint at height=%d hash=%s\n", mapBlockIndex[hashCheckpoint]->nHeight, hashCheckpoint.ToString());
    return true;
}

std::string CUnsignedSyncCheckpoint::ToString() const
{
    return strprintf(
                "CSyncCheckpoint(\n"
                "    nVersion       = %d\n"
                "    hashCheckpoint = %s\n"
                ")\n", nVersion, hashCheckpoint.ToString().c_str());
}

void CUnsignedSyncCheckpoint::print() const
{
    LogPrintf("%s", ToString());
}

uint256 CSyncCheckpoint::GetHash() const
{
    return SerializeHash(*this);
}

bool CSyncCheckpoint::RelayTo(CNode *pnode) const
{
    // returns true if wasn't already sent
    if (g_connman && pnode->hashCheckpointKnown != hashCheckpoint)
    {
        pnode->hashCheckpointKnown = hashCheckpoint;
        g_connman->PushMessage(pnode, CNetMsgMaker(pnode->GetSendVersion()).Make("checkpoint", *this));
        return true;
    }
    return false;
}
