// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"
#include <map>
#include <string>

namespace Consensus {

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which merged mining becomes active */
    int MMHeight;
    /** Block height at which V7 rules becomes active */
    int V7Height;
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    int64_t nTargetSpacing;
    int64_t nTargetTimespan;
    uint256 nMinimumChainTrust;
    uint256 defaultAssumeValid;

    // mfcoin stuff:
    uint256 bnInitialHashTarget;
    int64_t nStakeTargetSpacing;
    int64_t nTargetSpacingMax;
    int64_t nStakeMinAge;
    int64_t nStakeMaxAge;
    int64_t nStakeModifierInterval;

    /** Used to check majorities for block version upgrade */
    int nRejectBlockOutdatedMajority;
    int nToCheckBlockUpgradeMajority;

    /** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
    int nCoinbaseMaturity;
    int nCoinbaseMaturityOld;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
