// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the GPL3 software license, see the accompanying
// file COPYING or http://www.gnu.org/licenses/gpl.html.

#include "kernel.h"
#include "util.h"
#include "chainparams.h"
#include "validation.h"
#include "streams.h"
#include "uint256hm.h"
#include "wallet/wallet.h"
#include "timedata.h"
#include "consensus/validation.h"
#include "txdb.h"

using namespace std;

// Hard checkpoints of stake modifiers to ensure they are deterministic
static std::map<int, unsigned int> mapStakeModifierCheckpoints =
{
    { 0,     0x0e00670b }
};

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("%s: null pindex", __func__);
    do {
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifier = pindex->nStakeModifier;
            nModifierTime = pindex->GetBlockTime();
            return true;
        }
    } while ((pindex = pindex->pprev) != NULL);
    return error("%s: no generation at genesis block", __func__);
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert (nSection >= 0 && nSection < 64);
    return (Params().GetConsensus().nStakeModifierInterval * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionInterval()
{
    static int64_t nSelectionInterval = -1;
    if (nSelectionInterval < 0) {
        nSelectionInterval = 0;
        for (int nSection=0; nSection<64; nSection++)
            nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection);
    }
    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(
    vector<pair<const CBlockIndex*, arith_uint256> >& vSortedByTimestamp,
    int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev,
    const CBlockIndex** pindexSelected)
{
    arith_uint256 zero = 0;
    pair<const CBlockIndex*, arith_uint256> *itemSelected = NULL;

    for (auto& item : vSortedByTimestamp) {
        const CBlockIndex* pindex = item.first;
        if (pindex == NULL)
            continue; // This block already has been selected

        if (itemSelected != NULL && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;

        if (itemSelected == NULL || item.second < itemSelected->second) {
            // compute the selection hash by hashing its proof-hash and the
            // previous proof-of-stake modifier
            if (item.second == zero) {
                uint256 hashProof = pindex->IsProofOfStake()? pindex->hashProofOfStake : pindex->GetBlockHash();
                CDataStream ss(SER_GETHASH, 0);
                ss << hashProof << nStakeModifierPrev;
                item.second = UintToArith256(Hash(ss.begin(), ss.end()));
                // the selection hash is divided by 2**32 so that proof-of-stake block
                // is always favored over proof-of-work block. this is to preserve
                // the energy efficiency property
                if (pindex->IsProofOfStake())
                    item.second >>= 32;

                if (itemSelected != NULL && item.second >= itemSelected->second)
                    continue;
            }
            itemSelected = &item;
        }
    } // for
 
    if (itemSelected) {
        *pindexSelected = itemSelected->first;
        itemSelected->first = NULL;
        return true;
    } else {
        *pindexSelected = (const CBlockIndex*) 0;
        return false;
    }

  //  if (fDebug && GetBoolArg("-printstakemodifier", false))
  //      LogPrintf("%s: selection hash=%s\n", __func__, hashBest.ToString());
}

static void SwapSort(vector<pair<const CBlockIndex*, arith_uint256> > &v, unsigned pos) {
  if(pos >= v.size() - 1)
	  return;

  pair<const CBlockIndex*, arith_uint256> &a = v[pos];
  pair<const CBlockIndex*, arith_uint256> &b = v[pos + 1];

  if (a.first->GetBlockTime() < b.first->GetBlockTime())
    return;
  
  if (a.first->GetBlockTime() == b.first->GetBlockTime()) {
 
    const uint256& ha = a.first->GetBlockHash(); // needed because of weird g++ (5.4.0 20160609) bug
    const uint256& hb = b.first->GetBlockHash();
    const uint32_t *pa = ha.GetDataPtr();
    const uint32_t *pb = hb.GetDataPtr();
    int cnt = 256 / 32;
    for( ; ; ) {
      --cnt;
      if(pa[cnt] < pb[cnt])
	return;
      if(pa[cnt] > pb[cnt]) 
        break; // go to swap
      if(cnt == 0)
	return;
    }
  }

  std::swap(a, b);
  SwapSort(v, pos - 1);
  SwapSort(v, pos + 1);
} // SwapSort


// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every 
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(const CBlockIndex* pindexCurrent, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier)
{
    const CBlockIndex* pindexPrev = pindexCurrent->pprev;
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev)
    {
        fGeneratedStakeModifier = true;
        return true;  // genesis block's modifier is 0
    }
    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("%s: unable to get last modifier", __func__);
    static int fPrint =-1;
    if (fPrint < 0)
        fPrint = GetBoolArg("-printstakemodifier", false);

    if (fPrint)
        LogPrintf("%s: prev modifier=0x%016x time=%s epoch=%u\n", __func__, nStakeModifier, DateTimeStrFormat(nModifierTime), (unsigned int)nModifierTime);
    const Consensus::Params& params = Params().GetConsensus();
    if (nModifierTime / params.nStakeModifierInterval >= pindexPrev->GetBlockTime() / params.nStakeModifierInterval)
    // if (nModifierTime + params.nStakeModifierInterval > pindexPrev->GetBlockTime())
    {
        if (fPrint)
            LogPrintf("%s: no new interval keep current modifier: pindexPrev nHeight=%d nTime=%u\n", __func__, pindexPrev->nHeight, (unsigned int)pindexPrev->GetBlockTime());
        return true;
    }
    if (nModifierTime / params.nStakeModifierInterval >= pindexCurrent->GetBlockTime() / params.nStakeModifierInterval)
    // if (nModifierTime + params.nStakeModifierInterval > pindexCurrent->GetBlockTime())
    {
        // v0.4+ requires current block timestamp also be in a different modifier interval
        if (IsProtocolV04(pindexCurrent->nTime))
        {
            if (fPrint)
                LogPrintf("%s: (v0.3.5+) no new interval keep current modifier: pindexCurrent nHeight=%d nTime=%u\n", __func__, pindexCurrent->nHeight, (unsigned int)pindexCurrent->GetBlockTime());
            return true;
        }
        else if (fPrint)
                LogPrintf("%s: v0.3.4 modifier at block %s not meeting v0.4+ protocol: pindexCurrent nHeight=%d nTime=%u\n", __func__, pindexCurrent->GetBlockHash().ToString(), pindexCurrent->nHeight, (unsigned int)pindexCurrent->GetBlockTime());
    }

    // Sort candidate blocks by timestamp
    vector<pair<const CBlockIndex*, arith_uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * params.nStakeModifierInterval / params.nStakeTargetSpacing + 1);
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval();
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / params.nStakeModifierInterval) * params.nStakeModifierInterval - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart)
    {
        if (!mapBlockIndex.count(pindex->GetBlockHash()))
            return error("%s: failed to find block index for candidate block %s", __func__, pindex->GetBlockHash().ToString());
        vSortedByTimestamp.push_back(make_pair(pindex, 0));
        pindex = pindex->pprev;
    }

    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    for(int i = 0; i < (int)vSortedByTimestamp.size() - 1; i++)
	    SwapSort(vSortedByTimestamp, i);

#if 0

    vs2 = vSortedByTimestamp;
    reverse(vs2.begin(), vs2.end());
    for(int i = 0; i < vs2.size() - 1; i++)
	    SwapSort(vs2, i);


    // Shuffle before sort
    uint32_t rnd = GetRand(1 << 30);
    for (int i = vSortedByTimestamp.size() - 1, j = (i >> 2) | 1; i > j; --i)
        std::swap(vSortedByTimestamp[i], vSortedByTimestamp[(rnd = rnd * 22695477 + 1) % i]);


    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end(), [] (const pair<const CBlockIndex*, arith_uint256> &a, const pair<const CBlockIndex*, arith_uint256> &b)
    {
        if (a.first->GetBlockTime() != b.first->GetBlockTime())
            return a.first->GetBlockTime() < b.first->GetBlockTime();
        // Timestamp equals - compare block hashes
        const uint256& ha = a.first->GetBlockHash(); // needed because of weird g++ (5.4.0 20160609) bug
        const uint256& hb = b.first->GetBlockHash();
        const uint32_t *pa = ha.GetDataPtr();
        const uint32_t *pb = hb.GetDataPtr();
        int cnt = 256 / 32;
        do {
            --cnt;
            if(pa[cnt] != pb[cnt])
                return pa[cnt] < pb[cnt];
        } while(cnt);
        return false; // Elements are equal
    });


    // LogPrintf("DBG: ArrSZ=%u, sor1=%u sor2=%u\n", (unsigned)vSortedByTimestamp.size(), sor1, sor2);
#endif

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    for (int nRound=0; nRound<min(64, (int)vSortedByTimestamp.size()); nRound++)
    {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);
        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("%s: unable to select block at round %d", __func__, nRound);
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);
        // add the selected block from candidates to selected list
        if (fDebug && GetBoolArg("-printstakemodifier", false))
            LogPrintf("%s: selected round %d stop=%s height=%d bit=%d\n", __func__,
                nRound, DateTimeStrFormat(nSelectionIntervalStop), pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    if (fPrint)
        LogPrintf("%s: new modifier=0x%016x time=%s\n", __func__, nStakeModifierNew, DateTimeStrFormat(pindexPrev->GetBlockTime()));

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

struct StakeMod
{
    uint64_t nStakeModifier;
    int64_t nStakeModifierTime;
    int nStakeModifierHeight;
};

// V0.5: Stake modifier used to hash for a stake kernel is chosen as the stake
// modifier that is (nStakeMinAge minus a selection interval) earlier than the
// stake, thus at least a selection interval later than the coin generating the // kernel, as the generating coin is from at least nStakeMinAge ago.
static bool GetKernelStakeModifierV05(CBlockIndex* pindexPrev, unsigned int nTimeTx, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    const Consensus::Params& params = Params().GetConsensus();
    const CBlockIndex* pindex = pindexPrev;
    nStakeModifierHeight = pindex->nHeight;
    nStakeModifierTime = pindex->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval();

    if (nStakeModifierTime + params.nStakeMinAge - nStakeModifierSelectionInterval <= (int64_t) nTimeTx)
    {
        // Best block is still more than
        // (nStakeMinAge minus a selection interval) older than kernel timestamp
        if (fPrintProofOfStake)
            return error("GetKernelStakeModifier() : best block %s at height %d too old for stake",
                pindex->GetBlockHash().ToString(), pindex->nHeight);
        else
            return false;
    }
    // loop to find the stake modifier earlier by
    // (nStakeMinAge minus a selection interval)
    while (nStakeModifierTime + params.nStakeMinAge - nStakeModifierSelectionInterval >(int64_t) nTimeTx)
    {
        if (!pindex->pprev)
        {   // reached genesis block; should not happen
            return error("GetKernelStakeModifier() : reached genesis block");
        }
        pindex = pindex->pprev;
        if (pindex->GeneratedStakeModifier())
        {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
static bool GetKernelStakeModifierV03(CBlockIndex* pindexPrev, uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    const Consensus::Params& params = Params().GetConsensus();
#ifdef ENABLE_WALLET
    // init internal cache for stake modifiers (this is used only to reduce CPU usage)
    static uint256HashMap<StakeMod> StakeModCache;
    static bool initCache = false;
    if (initCache == false)
    {
        if (pwalletMain) {
            std::vector<COutput> vCoins;
            pwalletMain->AvailableCoins(vCoins, false);
            StakeModCache.Set(vCoins.size() + 1000);
        } else
            StakeModCache.Set(100);
        initCache = true;
    }

    // Clear cache after every new block.
    // If cache is not cleared here it will result in blockchain unable to be downloaded.
    static int nClrHeight = 0;
    bool fSameBlock;

    if (nClrHeight != pindexPrev->nHeight)
    {
        nClrHeight = pindexPrev->nHeight;
        StakeModCache.clear();
        fSameBlock = false;
    }
    else
    {
        uint256HashMap<StakeMod>::Data *pcache = StakeModCache.Search(hashBlockFrom);
        if (pcache != NULL)
        {
            nStakeModifier = pcache->value.nStakeModifier;
            nStakeModifierHeight = pcache->value.nStakeModifierHeight;
            nStakeModifierTime = pcache->value.nStakeModifierTime;
            return true;
        }
        fSameBlock = true;
    }
#endif

    nStakeModifier = 0;
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("%s: block not indexed", __func__);
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval();

    // mfcoin: we need to iterate index forward but we cannot use chainActive.Next()
    // because there is no guarantee that we are checking blocks in active chain.
    // So, we construct a temporary chain that we will iterate over.
    // pindexFrom - this block contains coins that are used to generate PoS
    // pindexPrev - this is a block that is previous to PoS block that we are checking, you can think of it as tip of our chain
    // tmpChain should contain all indexes in [pindexFrom..pindexPrev] (inclusive)
    std::vector<CBlockIndex*> tmpChain;
    int32_t nDepth = pindexPrev->nHeight - (pindexFrom->nHeight-1); // -1 is used to also include pindexFrom
    tmpChain.reserve(nDepth);
    CBlockIndex* it = pindexPrev;
    for (int i=1; i<=nDepth && !chainActive.Contains(it); i++) {
        tmpChain.push_back(it);
        it = it->pprev;
    }

    std::reverse(tmpChain.begin(), tmpChain.end());

    size_t n = 0;
    const CBlockIndex* pindex = pindexFrom;
    // loop to find the stake modifier later by a selection interval
    while (nStakeModifierTime < pindexFrom->GetBlockTime() + nStakeModifierSelectionInterval)
    {
        const CBlockIndex* old_pindex = pindex;
        pindex = (!tmpChain.empty() && pindex->nHeight >= tmpChain[0]->nHeight - 1)? tmpChain[n++] : chainActive.Next(pindex); 
        if (n > tmpChain.size() || pindex == NULL) // check if tmpChain[n+1] exists
        {   // reached best block; may happen if node is behind on block chain
            if (fPrintProofOfStake || (old_pindex->GetBlockTime() + params.nStakeMinAge - nStakeModifierSelectionInterval > GetAdjustedTime()))
                return error("%s: reached best block %s at height %d from block %s", __func__,
                    old_pindex->GetBlockHash().ToString(), old_pindex->nHeight, hashBlockFrom.ToString());
            else
                return false;
        }
        if (pindex->GeneratedStakeModifier())
        {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
    }
    nStakeModifier = pindex->nStakeModifier;

#ifdef ENABLE_WALLET
    // Save to cache only at minting phase
    if (fSameBlock)
    {
        struct StakeMod sm;
        sm.nStakeModifier = nStakeModifier;
        sm.nStakeModifierHeight = nStakeModifierHeight;
        sm.nStakeModifierTime = nStakeModifierTime;
        StakeModCache.Insert(hashBlockFrom, sm);
    }
#endif
    return true;
}

// Get the stake modifier specified by the protocol to hash for a stake kernel
static bool GetKernelStakeModifier(CBlockIndex* pindexPrev, uint256 hashBlockFrom, unsigned int nTimeTx, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    if (IsProtocolV05(nTimeTx, Params()))
        return GetKernelStakeModifierV05(pindexPrev, nTimeTx, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake);
    else
        return GetKernelStakeModifierV03(pindexPrev, hashBlockFrom, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake);
}

// ppcoin kernel protocol
// coinstake must meet hash target according to the protocol:
// kernel (input 0) must meet the formula
//     hash(nStakeModifier + txPrev.block.nTime + txPrev.offset + txPrev.nTime + txPrev.vout.n + nTime) < bnTarget * nCoinDayWeight
// this ensures that the chance of getting a coinstake is proportional to the
// amount of coin age one owns.
// The reason this hash is chosen is the following:
//   nStakeModifier: 
//       (v0.5) uses dynamic stake modifier around 21 days before the kernel,
//              versus static stake modifier about 9 days after the staked
//              coin (txPrev) used in v0.3
//       (v0.3) scrambles computation to make it very difficult to precompute
//              future proof-of-stake at the time of the coin's confirmation
//       (v0.2) nBits (deprecated): encodes all past block timestamps
//   txPrev.block.nTime: prevent nodes from guessing a good timestamp to
//                       generate transaction for future advantage
//   txPrev.offset: offset of txPrev inside block, to reduce the chance of 
//                  nodes generating coinstake at the same time
//   txPrev.nTime: reduce the chance of nodes generating coinstake at the same
//                 time
//   txPrev.vout.n: output number of txPrev, to reduce the chance of nodes
//                  generating coinstake at the same time
//   block/tx hash should not be used here as they can be generated in vast
//   quantities so as to generate blocks faster, degrading the system back into
//   a proof-of-work situation.
//
bool CheckStakeKernelHash(unsigned int nBits, CBlockIndex* pindexPrev, const CBlockHeader& blockFrom, unsigned int nTxPrevOffset, const CTransactionRef& txPrev, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake, bool fPrintProofOfStake)
{
    const Consensus::Params& params = Params().GetConsensus();
    if (nTimeTx < txPrev->nTime)  // Transaction timestamp violation
        return error("%s: nTime violation", __func__);

    unsigned int nTimeBlockFrom = blockFrom.GetBlockTime();
    if (txPrev->nTime + params.nStakeMinAge > nTimeTx) // Min age requirement
        return error("%s: min age violation", __func__);

    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);
    int64_t nValueIn = txPrev->vout[prevout.n].nValue;
    // v0.3 protocol kernel hash weight starts from 0 at the 30-day min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low
    int64_t nTimeWeight = min((int64_t)nTimeTx - txPrev->nTime, params.nStakeMaxAge) - (IsProtocolV03(nTimeTx)? params.nStakeMinAge : 0);
    arith_uint256 bnCoinDayWeight = arith_uint256(nValueIn) * nTimeWeight / COIN / (24 * 60 * 60);
    // Calculate hash
    CDataStream ss(SER_GETHASH, 0);
    uint64_t nStakeModifier = 0;
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;
    if (IsProtocolV03(nTimeTx))  // v0.3 protocol
    {
        if (!GetKernelStakeModifier(pindexPrev, blockFrom.GetHash(), nTimeTx, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake))
            return false;
        ss << nStakeModifier;
    }
    else // v0.2 protocol
    {
        ss << nBits;
    }

    ss << nTimeBlockFrom << nTxPrevOffset << txPrev->nTime << prevout.n << nTimeTx;

    if (nTimeTx >= 1489782503 && !IsProtocolV05(nTimeTx, Params())) // block 219831, until 06/18/2019
        ss << pindexPrev->GetBlockHash();

    hashProofOfStake = Hash(ss.begin(), ss.end());
    if (fPrintProofOfStake)
    {
        if (IsProtocolV03(nTimeTx))
            LogPrintf("%s: using modifier 0x%016x at height=%d timestamp=%s for block from height=%d timestamp=%s\n", __func__,
                nStakeModifier, nStakeModifierHeight,
                DateTimeStrFormat(nStakeModifierTime),
                mapBlockIndex[blockFrom.GetHash()]->nHeight,
                DateTimeStrFormat(blockFrom.GetBlockTime()));
        LogPrintf("%s: check protocol=%s modifier=0x%016x nTimeBlockFrom=%u nTxPrevOffset=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n", __func__,
            IsProtocolV05(nTimeTx, Params())? "0.5" : (IsProtocolV03(nTimeTx)? "0.3" : "0.2"),
            IsProtocolV03(nTimeTx)? nStakeModifier : (uint64_t) nBits,
            nTimeBlockFrom, nTxPrevOffset, txPrev->nTime, prevout.n, nTimeTx,
            hashProofOfStake.ToString());
    }

    // Now check if proof-of-stake hash meets target protocol
    if (UintToArith256(hashProofOfStake) > bnCoinDayWeight * bnTargetPerCoinDay)
        return false;
    if (fDebug && !fPrintProofOfStake)
    {
        if (IsProtocolV03(nTimeTx))
            LogPrintf("%s: using modifier 0x%016x at height=%d timestamp=%s for block from height=%d timestamp=%s\n", __func__,
                nStakeModifier, nStakeModifierHeight,
                DateTimeStrFormat(nStakeModifierTime),
                mapBlockIndex[blockFrom.GetHash()]->nHeight,
                DateTimeStrFormat(blockFrom.GetBlockTime()));
        LogPrintf("%s: pass protocol=%s modifier=0x%016x nTimeBlockFrom=%u nTxPrevOffset=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n", __func__,
            IsProtocolV03(nTimeTx)? "0.3" : "0.2",
            IsProtocolV03(nTimeTx)? nStakeModifier : (uint64_t) nBits,
            nTimeBlockFrom, nTxPrevOffset, txPrev->nTime, prevout.n, nTimeTx,
            hashProofOfStake.ToString());
    }
    return true;
}

// Check kernel hash target and coinstake signature
bool CheckProofOfStake(CValidationState& state, CBlockIndex* pindexPrev, const CTransactionRef& tx, unsigned int nBits, uint256& hashProofOfStake)
{
    if (!tx->IsCoinStake())
        return error("%s: called on non-coinstake %s", __func__, tx->GetHash().ToString());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txin = tx->vin[0];

    // Transaction index is required to get to block header
    if (!fTxIndex)
        return error("%s: transaction index not available", __func__);

    // First try finding the previous transaction in database
    CTransactionRef txTmp;
    uint256 hashBlock = uint256();
    if (!GetTransaction(txin.prevout.hash, txTmp, Params().GetConsensus(), hashBlock))
        // previous transaction not in main chain, may occur during initial download
        return state.DoS(1, false, REJECT_INVALID, "prev-tx-not-found", false, strprintf("%s: txPrev not found", __func__));

    // Verify signature
    CCoins coins(*txTmp, 0);
    PrecomputedTransactionData txdata(*tx);
    if (!CScriptCheck(coins, *tx, 0, SCRIPT_VERIFY_P2SH, true, &txdata)())
        return state.DoS(100, false, REJECT_INVALID, "invalid-pos-script", false, strprintf("%s: VerifyScript failed on coinstake %s", __func__, tx->GetHash().ToString()));

    // Get transaction index for the previous transaction
    CDiskTxPos postx;
    if (!pblocktree->ReadTxIndex(txin.prevout.hash, postx))
        return error("%s: tx index not found", __func__);  // tx index not found

    // Read txPrev and header of its block
    CBlockHeader header;
    CTransactionRef txPrev;
    {
        CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
        try {
            file >> header;
            fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
            file >> txPrev;
        } catch (std::exception& e) {
            return error("%s: deserialize or I/O error", __func__);
        }
        if (txPrev->GetHash() != txin.prevout.hash)
            return error("%s: txid mismatch", __func__);
    }

    if (!CheckStakeKernelHash(nBits, pindexPrev, header, postx.nTxOffset + CBlockHeader::NORMAL_SERIALIZE_SIZE, txPrev, txin.prevout, tx->nTime, hashProofOfStake, fDebug))
        // may occur during initial download or if behind on block chain sync
        return state.DoS(1, false, REJECT_INVALID, "invalid-stake-hash", false, strprintf("%s: INFO: check kernel failed on coinstake %s, hashProof=%s", __func__, tx->GetHash().ToString(), hashProofOfStake.ToString()));

    return true;
}

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx)
{
    if (IsProtocolV03(nTimeTx))  // v0.3 protocol
        return (nTimeBlock == nTimeTx);
    else // v0.2 protocol
        return ((nTimeTx <= nTimeBlock) && (nTimeBlock <= nTimeTx + nMaxClockDrift));
}

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex)
{
    assert (pindex->pprev || pindex->GetBlockHash() == Params().GetConsensus().hashGenesisBlock);
    // Hash previous checksum with flags, hashProofOfStake and nStakeModifier
    CDataStream ss(SER_GETHASH, 0);
    if (pindex->pprev)
        ss << pindex->pprev->nStakeModifierChecksum;
    ss << pindex->nFlags << pindex->hashProofOfStake << pindex->nStakeModifier;
    arith_uint256 hashChecksum = UintToArith256(Hash(ss.begin(), ss.end()));
    hashChecksum >>= (256 - 32);
    return hashChecksum.GetLow64();
}

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum)
{
    if (Params().NetworkIDString() != "main") return true; // Testnet or Regtest has no checkpoints
    if (mapStakeModifierCheckpoints.count(nHeight))
        return nStakeModifierChecksum == mapStakeModifierCheckpoints[nHeight];
    return true;
}
