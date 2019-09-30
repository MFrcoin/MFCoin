// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "validation.h"
#include "net.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"
#include "wallet/wallet.h"
#include "warnings.h"

#include <algorithm>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <queue>
#include <utility>
#include <stack>
#include <fstream>

//////////////////////////////////////////////////////////////////////////////
//
// MFCoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
uint64_t nLastBlockWeight = 0;
int64_t nLastCoinStakeSearchInterval = 0;

class ScoreCompare
{
public:
    ScoreCompare() {}

    bool operator()(const CTxMemPool::txiter a, const CTxMemPool::txiter b)
    {
        return CompareTxMemPoolEntryByScore()(*b,*a); // Convert to less than
    }
};

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    return nNewTime - nOldTime;
}

BlockAssembler::BlockAssembler(const CChainParams& _chainparams)
    : chainparams(_chainparams)
{
    // Block resource limits
    // If neither -blockmaxsize or -blockmaxweight is given, limit to DEFAULT_BLOCK_MAX_*
    // If only one is given, only restrict the specified resource.
    // If both are given, restrict both.
    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
    nBlockMaxSize = DEFAULT_BLOCK_MAX_SIZE;
    bool fWeightSet = false;
    if (IsArgSet("-blockmaxweight")) {
        nBlockMaxWeight = GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
        nBlockMaxSize = MAX_BLOCK_SERIALIZED_SIZE;
        fWeightSet = true;
    }
    if (IsArgSet("-blockmaxsize")) {
        nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
        if (!fWeightSet) {
            nBlockMaxWeight = nBlockMaxSize * WITNESS_SCALE_FACTOR;
        }
    }
    if (IsArgSet("-blockmintxfee")) {
        CAmount n = 0;
        ParseMoney(GetArg("-blockmintxfee", ""), n);
        blockMinFeeRate = CFeeRate(n);
    } else {
        blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }

    // Limit weight to between 4K and MAX_BLOCK_WEIGHT-4K for sanity:
    nBlockMaxWeight = std::max((unsigned int)4000, std::min((unsigned int)(MAX_BLOCK_WEIGHT-4000), nBlockMaxWeight));
    // Limit size to between 1K and MAX_BLOCK_SERIALIZED_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SERIALIZED_SIZE-1000), nBlockMaxSize));
    // Whether we need to account for byte usage (in addition to weight usage)
    fNeedSizeAccounting = (nBlockMaxSize < MAX_BLOCK_SERIALIZED_SIZE-1000);
}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockSize = 1000;
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;
    fIncludeWitness = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;

    lastFewTxs = 0;
    blockFinished = false;
}

// mfcoin: if pwallet != NULL it will attempt to create coinstake
std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, bool fMineWitnessTx, CWallet* pwallet, bool* pfPoSCancel)
{
    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    // ppcoin: if coinstake available add coinstake tx
    static int64_t nLastCoinStakeSearchTime = GetAdjustedTime();  // only initialized at startup

    LOCK(cs_main);
    CBlockIndex* pindexPrev = chainActive.Tip();

    bool fV7Enabled = IsV7Enabled(pindexPrev, chainparams.GetConsensus());
    nHeight = pindexPrev->nHeight + 1;

    if (pwallet)  // attemp to find a coinstake
    {
        *pfPoSCancel = true;
        pblock->nBits = GetNextTargetRequired(pindexPrev, true, chainparams.GetConsensus());
        CMutableTransaction txCoinStake;
        int64_t nSearchTime = txCoinStake.nTime; // search to current time
        if (nSearchTime > nLastCoinStakeSearchTime)
        {
            if (pwallet->CreateCoinStake(*pwallet, pblock->nBits, nSearchTime-nLastCoinStakeSearchTime, txCoinStake, fV7Enabled, nHeight))
            {
                if (txCoinStake.nTime >= std::max(pindexPrev->GetMedianTimePast()+1, pindexPrev->GetBlockTime() - nMaxClockDrift))
                {   // make sure coinstake would meet timestamp protocol
                    // as it would be the same as the block timestamp
                    coinbaseTx.vout[0].SetEmpty();
                    coinbaseTx.nTime = txCoinStake.nTime;
                    pblock->vtx.push_back(MakeTransactionRef(CTransaction(txCoinStake)));
                    *pfPoSCancel = false;
                }
            }
            nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
            nLastCoinStakeSearchTime = nSearchTime;
        }
        if (*pfPoSCancel)
            return nullptr; // mfcoin: there is no point to continue if we failed to create coinstake
    }
    else
        pblock->nBits = GetNextTargetRequired(pindexPrev, false, chainparams.GetConsensus());

    LOCK(mempool.cs);

    pblock->SetBlockVersion(GetArg("-blockversion", CBlockHeader::CURRENT_VERSION));

    pblock->nTime = GetAdjustedTime();
    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    // Decide whether to include witness transactions
    // This is only needed in case the witness softfork activation is reverted
    // (which would require a very deep reorganization) or when
    // -promiscuousmempoolflags is used.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
    fIncludeWitness = fV7Enabled && fMineWitnessTx;

    addTxs();

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    nLastBlockWeight = nBlockWeight;

    // Compute final coinbase transaction.
    if (pblock->IsProofOfWork())
        coinbaseTx.vout[0].nValue = GetProofOfWorkReward(pblock->nBits, fV7Enabled, nHeight);
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;
    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    if (fIncludeWitness)
        pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock);
    pblocktemplate->vTxFees[0] = -nFees;

    uint64_t nSerializeSize = GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION);
    LogPrintf("CreateNewBlock(): total size: %u block weight: %u txs: %u fees: %ld sigops %d\n", nSerializeSize, GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost);

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    if (pblock->IsProofOfStake())
        pblock->nTime      = pblock->vtx[1]->nTime; //same as coinstake timestamp
    pblock->nTime          = std::max(pindexPrev->GetMedianTimePast()+1, pblock->GetMaxTransactionTime());
    pblock->nTime          = std::max(pblock->GetBlockTime(), pindexPrev->GetBlockTime() - nMaxClockDrift);
    if (pblock->IsProofOfWork())
        UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);
    if (pblock->IsProofOfWork())
        pblock->mtpHashData = std::make_shared<CMTPHashData>();
    else
        pblock->mtpHashData = nullptr;

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false, false)) {   // mfcoin: we do not check block signature here, since we did not sign it yet
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
    }
    int64_t nTime2 = GetTimeMicros();

    LogPrint("bench", "CreateNewBlock() txs: %.2fms, validity: %.2fms (total %.2fms)\n", 0.001 * (nTime1 - nTimeStart), 0.001 * (nTime2 - nTime1), 0.001 * (nTime2 - nTimeStart));

    return std::move(pblocktemplate);
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewForkBlock(const CScript& scriptPubKeyIn, bool& bFileNotFound)
{
    /*
      Because CreateNewForkBlock does file io when
      reading utxos, we grab the main lock to peek
      at the tip, release it to read the file and
      fill in the template and then reacquire it
      at the end to check if the active tip changed
      while we were doing the above
     */
    resetBlock();
    pblocktemplate = nullptr;
    //CBlockTemplate * ret = pblocktemplate.get();
    int tipHeight;
    int snappedHeight;
    //const CChainParams& chainparams = Params();

    {
        LOCK(cs_main);
        tipHeight = chainActive.Tip()->nHeight;
    }

    do
    {
        snappedHeight = tipHeight;
        pblocktemplate = CreateNewForkBlock(scriptPubKeyIn, bFileNotFound, snappedHeight + 1);
        pblock = &pblocktemplate->block; // pointer for convenience
        {
            LOCK(cs_main);
            CBlockIndex* pindexPrev = chainActive.Tip();
            //CBlock *pblock = &ret->block; // pointer for convenience

            tipHeight = pindexPrev->nHeight;
            if(tipHeight != snappedHeight)
            {
                LogPrintf("WARN: tip changed from %u to %u while generating block template\n",
                          snappedHeight, tipHeight);
                continue;
            }

            // if we are good then fill in the final details
            pblock->hashPrevBlock = pindexPrev->GetBlockHash();
            UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
            pblock->nBits = GetNextTargetRequired(pindexPrev, false, chainparams.GetConsensus());
            pblock->SetBlockVersion(GetArg("-blockversion", CBlockHeader::CURRENT_VERSION));
            pblock->nTime          = std::max(pindexPrev->GetMedianTimePast()+1, pblock->GetMaxTransactionTime());
            pblock->nTime          = std::max(pblock->GetBlockTime(), pindexPrev->GetBlockTime() - nMaxClockDrift);
            if (pblock->IsProofOfWork())
                UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);

            CValidationState state;
            if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false, false))
                throw std::runtime_error("CreateNewForkBlock(): TestBlockValidity failed");
        }
    } while(snappedHeight != tipHeight);

    return std::move(pblocktemplate);
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewForkBlock(const CScript& scriptPubKeyIn, bool& bFileNotFound, const int nHeight)
{
    //const CChainParams& chainparams = Params();
    const int nForkHeight = nHeight - forkStartHeight;

    std::string utxo_file_path = GetUTXOFileName(nHeight);
    std::ifstream if_utxo(utxo_file_path, std::ios::binary | std::ios::in);
    if (!if_utxo.is_open()) {
        bFileNotFound = true;
        LogPrintf("ERROR: CreateNewForkBlock(): [%u, %u of %u]: Cannot open UTXO file - %s\n",
                  nHeight, nForkHeight, forkHeightRange, utxo_file_path);
        return NULL;
    }

    // Create new block
    pblocktemplate.reset(new CBlockTemplate());
    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;

    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK(cs_main);
    CBlockIndex* pindexPrev = chainActive.Tip();
    bool fv7Enabled = IsV7Enabled(pindexPrev, chainparams.GetConsensus());

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = (unsigned int)(MAX_BLOCKFILE_SIZE-1000);

    uint64_t nBlockTotalAmount = 0;
    uint64_t nBlockSize = 1000;
    uint64_t nBlockTx = 0;
    uint64_t nBlockSigOps = 100;

    while (if_utxo && nBlockTx < forkCBPerBlock) {
        char term = 0;

        char coin[8] = {};
        if (!if_utxo.read(coin, 8)) {
            // the last file may be shorter than forkCBPerBlock
            if(!if_utxo.eof() || nForkHeight != forkHeightRange)
                LogPrintf("ERROR: CreateNewForkBlock(): [%u, %u of %u]: UTXO file corrupted? - No more data (Amount)\n",
                          nHeight, nForkHeight, forkHeightRange);
            break;
        }

        char pubkeysize[8] = {};
        if (!if_utxo.read(pubkeysize, 8)) {
            LogPrintf("ERROR: CreateNewForkBlock(): [%u, %u of %u]: UTXO file corrupted? - Not more data (PubKeyScript size)\n",
                      nHeight, nForkHeight, forkHeightRange);
            break;
        }

        int pbsize = bytes2uint64(pubkeysize);
        if (pbsize == 0) {
            LogPrintf("ERROR: CreateNewForkBlock(): [%u, %u of %u]: UTXO file corrupted? -  PubKeyScript size = 0\n",
                      nHeight, nForkHeight, forkHeightRange);
            //but proceed
        }

        std::unique_ptr<char[]> pubKeyScript(new char[pbsize]);
        if (!if_utxo.read(&pubKeyScript[0], pbsize)) {
            LogPrintf("ERROR: CreateNewForkBlock(): [%u, %u of %u]: UTXO file corrupted? Not more data (PubKeyScript)\n",
                      nHeight, nForkHeight, forkHeightRange);
            break;
        }

        char timestamp_bytes[8] = {};
        if (!if_utxo.read(timestamp_bytes, 8)) {
            LogPrintf("ERROR: AddUtxoTransactions(): UTXO file corrupted? - Not more data (Timestamp)\n");
            break;
        }

        uint64_t timestamp = bytes2uint64(timestamp_bytes);
        uint64_t amount = bytes2uint64(coin);

        // Add coinbase tx's
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vin[0].prevout.SetNull();
        txNew.vout.resize(1);
        txNew.nTime = timestamp;

        unsigned char* pks = (unsigned char*)pubKeyScript.get();
        txNew.vout[0].scriptPubKey = CScript(pks, pks+pbsize);

        txNew.vout[0].nValue = amount;
        if(nBlockTx == 0)
            txNew.vin[0].scriptSig = CScript() << nHeight << CScriptNum(nBlockTx) << ToByteVector(hashPid) << OP_0;
        else
            txNew.vin[0].scriptSig = CScript() << nHeight << CScriptNum(nBlockTx) << OP_0;

        unsigned int nTxSize = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
        if (nBlockSize + nTxSize >= nBlockMaxSize)
        {
            LogPrintf("ERROR:  CreateNewForkBlock(): [%u, %u of %u]: %u: block would exceed max size\n",
                      nHeight, nForkHeight, forkHeightRange, nBlockTx);
            break;
        }

        // Legacy limits on sigOps:
        unsigned int nTxSigOps = GetLegacySigOpCount(txNew);
        pblock->vtx.push_back(MakeTransactionRef(CTransaction(txNew)));
        nBlockSize += nTxSize;
        nBlockSigOps += nTxSigOps;
        nBlockTotalAmount += amount;
        ++nBlockTx;

        if (!if_utxo.read(&term, 1) || term != '\n') {
            LogPrintf("ERROR:  CreateNewForkBlock(): [%u, %u of %u]: invalid record separator\n",
                      nHeight, nForkHeight, forkHeightRange);
            break;
        }
    }
    LogPrintf("CreateNewForkBlock(): [%u, %u of %u]: txns=%u size=%u amount=%u sigops=%u\n",
              nHeight, nForkHeight, forkHeightRange, nBlockTx, nBlockSize, nBlockTotalAmount, nBlockSigOps);

    // Compute final coinbase transaction.
    if (pblock->IsProofOfWork())
        coinbaseTx.vout[0].nValue = GetProofOfWorkReward(pblock->nBits, fv7Enabled, nHeight);
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;
    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    if (fIncludeWitness)
        pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock);
    pblocktemplate->vTxFees[0] = -nFees;
    //pblocktemplate->vTxFees[0] = 0;
    //pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * nTxSigOps;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);
    pblock->mtpHashData = std::make_shared<CMTPHashData>();
/*    uint64_t nonce = 0;
    while (nonce == 0) {
        GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
    }*/
    pblock->nNonce = 0;

    return std::move(pblocktemplate);
}

bool BlockAssembler::isStillDependent(CTxMemPool::txiter iter)
{
    BOOST_FOREACH(CTxMemPool::txiter parent, mempool.GetMemPoolParents(iter))
    {
        if (!inBlock.count(parent)) {
            return true;
        }
    }
    return false;
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost)
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature witness (in case segwit transactions are added to mempool before
//   segwit activation)
// - serialized size (in case -blockmaxsize is in use)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    uint64_t nPotentialBlockSize = nBlockSize; // only used with fNeedSizeAccounting
    BOOST_FOREACH (const CTxMemPool::txiter it, package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && it->GetTx().HasWitness())
            return false;
        if (fNeedSizeAccounting) {
            uint64_t nTxSize = ::GetSerializeSize(it->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
            if (nPotentialBlockSize + nTxSize >= nBlockMaxSize) {
                return false;
            }
            nPotentialBlockSize += nTxSize;
        }

        // ppcoin: timestamp limit
        if (it->GetTx().nTime > GetAdjustedTime() || (pblock->IsProofOfStake() && it->GetTx().nTime > pblock->vtx[1]->nTime))
            return false;
    }
    return true;
}

bool BlockAssembler::TestForBlock(CTxMemPool::txiter iter)
{
    if (nBlockWeight + iter->GetTxWeight() >= nBlockMaxWeight) {
        // If the block is so close to full that no more txs will fit
        // or if we've tried more than 50 times to fill remaining space
        // then flag that the block is finished
        if (nBlockWeight >  nBlockMaxWeight - 400 || lastFewTxs > 50) {
             blockFinished = true;
             return false;
        }
        // Once we're within 4000 weight of a full block, only look at 50 more txs
        // to try to fill the remaining space.
        if (nBlockWeight > nBlockMaxWeight - 4000) {
            lastFewTxs++;
        }
        return false;
    }

    if (fNeedSizeAccounting) {
        if (nBlockSize + ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION) >= nBlockMaxSize) {
            if (nBlockSize >  nBlockMaxSize - 100 || lastFewTxs > 50) {
                 blockFinished = true;
                 return false;
            }
            if (nBlockSize > nBlockMaxSize - 1000) {
                lastFewTxs++;
            }
            return false;
        }
    }

    if (nBlockSigOpsCost + iter->GetSigOpCost() >= MAX_BLOCK_SIGOPS_COST) {
        // If the block has room for no more sig ops then
        // flag that the block is finished
        if (nBlockSigOpsCost > MAX_BLOCK_SIGOPS_COST - 8) {
            blockFinished = true;
            return false;
        }
        // Otherwise attempt to find another tx with fewer sigops
        // to put in the block.
        return false;
    }

    // Must check that lock times are still valid
    // This can be removed once MTP is always enforced
    // as long as reorgs keep the mempool consistent.
    if (!IsFinalTx(iter->GetTx(), nHeight, nLockTimeCutoff))
        return false;

    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    if (fNeedSizeAccounting) {
        nBlockSize += ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
    }
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        double dPriority = iter->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(iter->GetTx().GetHash(), dPriority, dummy);
        LogPrintf("priority %.1f fee %s txid %s\n",
                  dPriority,
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
        indexed_modified_transaction_set &mapModifiedTx)
{
    int nDescendantsUpdated = 0;
    BOOST_FOREACH(const CTxMemPool::txiter it, alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        BOOST_FOREACH(CTxMemPool::txiter desc, descendants) {
            if (alreadyAdded.count(desc))
                continue;
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCostWithAncestors -= it->GetSigOpCost();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
    return nDescendantsUpdated;
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    if (mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it))
        return true;
    return false;
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, CTxMemPool::txiter entry, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty())
    {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
                SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareModifiedEntry()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        for (size_t i=0; i<sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void BlockAssembler::addTxs()
{
    // How much of the block should be dedicated to transactions
    unsigned int nBlockTxSize = nBlockMaxSize;

    bool fSizeAccounting = fNeedSizeAccounting;
    fNeedSizeAccounting = true;

    std::stack<CTxMemPool::txiter> stack;
    std::set<CTxMemPool::txiter, CTxMemPool::CompareIteratorByHash> waitSet;
    typedef std::set<CTxMemPool::txiter, CTxMemPool::CompareIteratorByHash>::iterator waitIter;

    // fill stack with elements that are sorted by time with descending order.
    // note: to convert reverse iterator to forward iterator we use base()
    // but we need to add +1 to get iterator for the same element
    // operator+ is not supported for reverse iterators in boost
    // that is why we use ++ operator instead
    for (auto mi = mempool.mapTx.get<2>().rbegin(); mi != mempool.mapTx.get<2>().rend();)
        stack.push(mempool.mapTx.project<0>((++mi).base()));

    CTxMemPool::txiter iter;
    while (!stack.empty() && !blockFinished) { // add a tx from stack to fill the nBlockTxSize
        iter = stack.top();
        stack.pop();

        // If tx already in block, skip
        if (inBlock.count(iter)) {
            assert(false); // shouldn't happen
            continue;
        }

        // cannot accept witness transactions into a non-witness block
        if (!fIncludeWitness && iter->GetTx().HasWitness())
            continue;

        // If tx is dependent on other mempool txs which haven't yet been included
        // then put it in the waitSet
        if (isStillDependent(iter)) {
            waitSet.insert(iter);
            continue;
        }

        // ppcoin: timestamp limit
        if (iter->GetTx().nTime > GetAdjustedTime() || (pblock->IsProofOfStake() && iter->GetTx().nTime > pblock->vtx[1]->nTime))
            continue;

        // If this tx fits in the block add it, otherwise keep looping
        if (TestForBlock(iter)) {
            AddToBlock(iter);

            // If now that this txs is added we've surpassed our desired priority size
            if (nBlockSize >= nBlockTxSize) {
                break;
            }

            // This tx was successfully added, so
            // add transactions that depend on this one to the stack to try again
            BOOST_FOREACH(CTxMemPool::txiter child, mempool.GetMemPoolChildren(iter))
            {
                waitIter witer = waitSet.find(child);
                if (witer != waitSet.end()) {
                    stack.push(child);
                    waitSet.erase(witer);
                }
            }
        }
    }
    fNeedSizeAccounting = fSizeAccounting;
}

void BlockAssembler::addPriorityTxs()
{
    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    if (nBlockPrioritySize == 0) {
        return;
    }

    bool fSizeAccounting = fNeedSizeAccounting;
    fNeedSizeAccounting = true;

    // This vector will be sorted into a priority queue:
    std::vector<TxCoinAgePriority> vecPriority;
    TxCoinAgePriorityCompare pricomparer;
    std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash> waitPriMap;
    typedef std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash>::iterator waitPriIter;
    double actualPriority = -1;

    vecPriority.reserve(mempool.mapTx.size());
    for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
         mi != mempool.mapTx.end(); ++mi)
    {
        double dPriority = mi->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(mi->GetTx().GetHash(), dPriority, dummy);
        vecPriority.push_back(TxCoinAgePriority(dPriority, mi));
    }
    std::make_heap(vecPriority.begin(), vecPriority.end(), pricomparer);

    CTxMemPool::txiter iter;
    while (!vecPriority.empty() && !blockFinished) { // add a tx from priority queue to fill the blockprioritysize
        iter = vecPriority.front().second;
        actualPriority = vecPriority.front().first;
        std::pop_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
        vecPriority.pop_back();

        // If tx already in block, skip
        if (inBlock.count(iter)) {
            assert(false); // shouldn't happen for priority txs
            continue;
        }

        // cannot accept witness transactions into a non-witness block
        if (!fIncludeWitness && iter->GetTx().HasWitness())
            continue;

        // If tx is dependent on other mempool txs which haven't yet been included
        // then put it in the waitSet
        if (isStillDependent(iter)) {
            waitPriMap.insert(std::make_pair(iter, actualPriority));
            continue;
        }

        // ppcoin: timestamp limit
        if (iter->GetTx().nTime > GetAdjustedTime() || (pblock->IsProofOfStake() && iter->GetTx().nTime > pblock->vtx[1]->nTime))
            continue;

        // If this tx fits in the block add it, otherwise keep looping
        if (TestForBlock(iter)) {
            AddToBlock(iter);

            // If now that this txs is added we've surpassed our desired priority size
            // or have dropped below the AllowFreeThreshold, then we're done adding priority txs
            if (nBlockSize >= nBlockPrioritySize || !AllowFree(actualPriority)) {
                break;
            }

            // This tx was successfully added, so
            // add transactions that depend on this one to the priority queue to try again
            BOOST_FOREACH(CTxMemPool::txiter child, mempool.GetMemPoolChildren(iter))
            {
                waitPriIter wpiter = waitPriMap.find(child);
                if (wpiter != waitPriMap.end()) {
                    vecPriority.push_back(TxCoinAgePriority(wpiter->second,child));
                    std::push_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                    waitPriMap.erase(wpiter);
                }
            }
        }
    }
    fNeedSizeAccounting = fSizeAccounting;
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

static bool ProcessBlockFound(const CBlock* pblock, const CChainParams& chainparams)
{
    //LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0]->vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("MFCoinMiner: generated block is stale");
    }

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
    if (!ProcessNewBlock(Params(), shared_pblock, true, NULL))
        return error("ProcessNewBlock, block not accepted");

    return true;
}

#ifdef ENABLE_WALLET
void PoSMiner(CWallet *pwallet)
{
    LogPrintf("PoSMiner started for proof-of-stake\n");
    RenameThread("mfcoin-stake-minter");

    unsigned int nExtraNonce = 0;

    boost::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);

    // Compute timeout for pos as sqrt(numUTXO)
    unsigned int pos_timio;
    {
        std::vector<COutput> vCoins;
        pwallet->AvailableCoins(vCoins, false);
        pos_timio = GetArg("-staketimio", 500) + 30 * sqrt(vCoins.size());
        LogPrintf("Set proof-of-stake timeout: %ums for %u UTXOs\n", pos_timio, vCoins.size());
    }

    std::string strMintMessage = _("Info: Minting suspended due to locked wallet.");

    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");

        while (true) {
            while (pwallet->IsLocked()) {
                if (strMintWarning != strMintMessage) {
                    strMintWarning = strMintMessage;
                    uiInterface.NotifyAlertChanged(uint256(), CT_UPDATED);
                }
                MilliSleep(3000);
            }
            strMintWarning = "";  // clear locked wallet warning
            uiInterface.NotifyAlertChanged(uint256(), CT_UPDATED);
            if (Params().MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                while(g_connman == nullptr || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0 || IsInitialBlockDownload())
                    MilliSleep(1 * 60 * 1000);
            }

            //
            // Create new block
            //
            CBlockIndex* pindexPrev = chainActive.Tip();

            bool fPoSCancel = false;
            std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(coinbaseScript->reserveScript, true, pwallet, &fPoSCancel));
            if (!pblocktemplate.get())
            {
                if (fPoSCancel == true)
                {
                    MilliSleep(pos_timio);
                    continue;
                }
                LogPrintf("Error in PoSMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            // ppcoin: if proof-of-stake block found then process block
            if (pblock->IsProofOfStake())
            {
                {
                    LOCK2(cs_main, pwallet->cs_wallet);
                    if (!SignBlock(*pblock, *pwallet))
                    {
                        LogPrintf("PoSMiner(): failed to sign PoS block\n");
                        continue;
                    }
                }
                LogPrintf("CPUMiner : proof-of-stake block found %s\n", pblock->GetHash().ToString());
                ProcessBlockFound(pblock, Params());
                // Rest for ~3 minutes after successful block to preserve close quick
                MilliSleep(20 * 1000 + GetRand(40 * 1000));
            }
            MilliSleep(pos_timio);

            continue;
        }
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("PoSMiner terminated\n");
    return;
        // throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("PoSMiner runtime error: %s\n", e.what());
        return;
    }
}

// ppcoin: stake minter thread
void static ThreadStakeMinter(void* parg)
{
    LogPrintf("ThreadStakeMinter started\n");
    CWallet* pwallet = (CWallet*)parg;
    try
    {
        PoSMiner(pwallet);
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "ThreadStakeMinter()");
    } catch (...) {
        PrintExceptionContinue(NULL, "ThreadStakeMinter()");
    }
    LogPrintf("ThreadStakeMinter exiting\n");
}

// ppcoin: stake minter
void MintStake(boost::thread_group& threadGroup, CWallet* pwallet)
{
    // ppcoin: mint proof-of-stake blocks in the background
    threadGroup.create_thread(boost::bind(&ThreadStakeMinter, pwallet));
}
#endif // ENABLE_WALLET


//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

//
// ScanHash scans nonces looking for a hash with at least some zero bits.
// The nonce is usually preserved between calls, but periodically or if the
// nonce is 0xffff0000 or above, the block is rebuilt and nNonce starts over at
// zero.
//
bool static ScanHash(const CBlockHeader *pblock, uint32_t& nNonce, uint256 *phash)
{
    // Write the first 76 bytes of the block header to a double-SHA256 state.
    CHash256 hasher;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *pblock;
    assert(ss.size() == 80);
    hasher.Write((unsigned char*)&ss[0], 76);

    while (true) {
        nNonce++;

        // Write the last 4 bytes of the block header (the nonce) to a copy of
        // the double-SHA256 state, and compute the result.
        CHash256(hasher).Write((unsigned char*)&nNonce, 4).Finalize((unsigned char*)phash);

        // Return the nonce if the hash has at least some zero bits,
        // caller will check if it has enough to reach the target
        if (((uint16_t*)phash)[15] == 0)
            return true;

        // If nothing found after trying for a while, return -1
        if ((nNonce & 0xfff) == 0)
            return false;
    }
}

bool static ScanHashMTP(CBlockHeader *pblock, uint32_t& nNonce, uint256 *phash)
{
    *phash = mtp::hash(*pblock, Params().GetConsensus().powLimit);
    nNonce = pblock->nNonce;
    return true;

    // Return the nonce if the hash has at least some zero bits,
    // caller will check if it has enough to reach the target
    //if (((uint16_t *) phash)[15] == 0)
    //    return true;

    // If nothing found after trying for a while, return -1
    //if ((nNonce & 0xfff) == 0)
    //    return false;
}

void static MFCoinMiner(const CChainParams& chainparams)
{
    LogPrintf("MFCoinMiner started\n");
    RenameThread("mfcoin-miner");

    unsigned int nExtraNonce = 0;

    boost::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);

    bool bForkModeStarted = false;
    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");

        while (true) {
            if (chainparams.MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                while(g_connman == nullptr || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0)
                    MilliSleep(1000);
            }

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            std::unique_ptr<CBlockTemplate> pblocktemplate;
            CBlock *pblock = nullptr;

            bool isNextBlockFork = isForkBlock(pindexPrev->nHeight+1);

            if (isNextBlockFork) {
                if (!bForkModeStarted) {
                    LogPrintf("MFCoin Miner: switching into fork mode\n");
                    bForkModeStarted = true;
                }

                bool bFileNotFound = false;
                pblocktemplate = BlockAssembler(Params()).CreateNewForkBlock(coinbaseScript->reserveScript, bFileNotFound);
                if (!pblocktemplate.get()) {
                    if (bFileNotFound) {
                        MilliSleep(1000);
                        continue;
                    } else {
                        LogPrintf("Error in MFCoin Miner: Cannot create Fork Block\n");
                        return;
                    }
                }
                pblock = &pblocktemplate->block;
                pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);

                LogPrintf("Running MFCoin Miner with %u forking transactions in block (%u bytes)\n",
                          pblock->vtx.size(),
                          ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));
            } else {
                //if not in forking mode and/or provided file is read to the end - exit
                if (bForkModeStarted) {
                    LogPrintf("MFCoin Miner: Fork is done - switching back to regular miner\n");
                    bForkModeStarted = false;
                }

                pblocktemplate = BlockAssembler(Params()).CreateNewBlock(coinbaseScript->reserveScript, true, nullptr, nullptr);
                if (!pblocktemplate.get()) {
                    LogPrintf(
                            "Error in MFCoinMiner: Keypool ran out, please call keypoolrefill before restarting the mining hread\n");
                    return;
                }
                pblock = &pblocktemplate->block;
                IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

                LogPrintf("Running MFCoinMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                          ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));
            }
            //
            // Search
            //
            int64_t nStart = GetTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
            uint256 mtphash;
            uint32_t nNonce = 0;
            while (true) {
                // Check if something found
                if (ScanHashMTP(pblock, nNonce, &mtphash))
                {
                    if (UintToArith256(mtphash) <= hashTarget)
                    {
                        // Found a solution
                        pblock->nNonce = nNonce;
                        pblock->mtpHashValue = mtphash;
                        uint256 mtp;
                        mtp::verify(pblock->nNonce, *pblock, Params().GetConsensus().powLimit, &mtp);
                        assert(mtphash == mtp);

                        LOCK2(cs_main, pwalletMain->cs_wallet);
                        if (!SignBlock(*pblock, *pwalletMain))
                        {
                            LogPrintf("PoSMiner(): failed to sign PoS block");
                            continue;
                        }

                        LogPrintf("MFCoinMiner:\n");
                        LogPrintf("proof-of-work found  \n mtphash: %s hash: %s  \ntarget: %s\n", pblock->mtpHashValue.GetHex(), pblock->GetHash().GetHex(), hashTarget.GetHex());
                        ProcessBlockFound(pblock, chainparams);
                        coinbaseScript->KeepScript();

                        // In regression test mode, stop mining after a block is found.
                        if (chainparams.MineBlocksOnDemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if ((g_connman == nullptr || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0) && chainparams.MiningRequiresPeers())
                    break;
                if (nNonce >= 0xffff0000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nTime every few seconds
                if (UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev) < 0)
                    break; // Recreate the block if the clock has run backwards,
                           // so that we can use the correct time.
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("MFCoinMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("MFCoinMiner runtime error: %s\n", e.what());
        return;
    }
}

void GenerateMFCoins(bool fGenerate, int nThreads, const CChainParams& chainparams)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0)
        nThreads = GetNumCores();

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&MFCoinMiner, boost::cref(chainparams)));
}
