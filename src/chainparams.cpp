// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>
#include <boost/filesystem.hpp>

#include "chainparamsseeds.h"

#include "arith_uint256.h"
#include "streams.h"
#include "clientversion.h"
#include "hash.h"


bool CheckProofOfWork2(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

struct thread_data {
   CBlock block;
   int32_t nTime;
   arith_uint256 target;
   Consensus::Params consensus;
};

void *SolveBlock(void *threadarg)
{
    struct thread_data *my_data;
    my_data = (struct thread_data *) threadarg;
    CBlock& block = my_data->block;
    int32_t& nTime = my_data->nTime;
    arith_uint256& target = my_data->target;

    block.nTime = nTime;
    block.nNonce = 0;
    bool ret;
    while (UintToArith256(block.GetHash()) > target) {
        if (block.nNonce == 2147483647)
            break;
        ++block.nNonce;
    }
    ret = CheckProofOfWork2(block.GetHash(), block.nBits, my_data->consensus);
    printf("!!!solved: ret=%d nNonce=%d, nTime=%d\n", ret, block.nNonce, block.nTime);
    assert(false);
    pthread_exit(NULL);
}

void MineGenesisBlock(const CBlock& genesis, const Consensus::Params& consensus)
{
    const int NUM_THREADS = 8;

    pthread_t threads[NUM_THREADS];
    struct thread_data td[NUM_THREADS];
    pthread_attr_t attr;
    void *status;
    int rc;
    int i = 0;

    // Initialize and set thread joinable
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    for( i = 0; i < NUM_THREADS; i++ ) {
       printf("main() : creating thread\n");
       td[i].block = genesis;
       td[i].nTime = genesis.nTime+i;
       td[i].target = UintToArith256(consensus.bnInitialHashTarget);
       td[i].consensus = consensus;
       rc = pthread_create(&threads[i], &attr, SolveBlock, (void *)&td[i]);

       if (rc) {
          printf("Error:unable to create thread\n");
          exit(-1);
       }
    }

    // free attribute and wait for the other threads
    pthread_attr_destroy(&attr);
    for( i = 0; i < NUM_THREADS; i++ ) {
       rc = pthread_join(threads[i], &status);
    }
}

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTimeTx, uint32_t nTimeBlock, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(9999) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;
    txNew.nTime = nTimeTx;

    CBlock genesis;
    genesis.nTime    = nTimeBlock;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTimeTx, uint32_t nTimeBlock, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "ANIMA = VISITA INTERIORA TERRAE RECTIFICANDO INUENIES OCCULTUM LAPIDEM = SPIRITUS + CORPUS";
    const CScript genesisOutputScript = CScript();
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTimeTx, nTimeBlock, nNonce, nBits, nVersion, genesisReward);
}

void printGenesisParams(const CBlock& genesis, bool print_block = false)
{
    std::cout << "nonce: " << genesis.nNonce << '\n';
    std::cout << "mtp: " << genesis.mtpHashValue.GetHex() << '\n';
    std::cout << "hash: " << genesis.GetHash().GetHex() << '\n';
    std::cout << "merkle: " << genesis.hashMerkleRoot.GetHex() << '\n';
    if (print_block)
        std::cout << "Block: " << genesis.ToString() << '\n';
}

uint256 mineMtp(CBlock& genesis, uint256 powLimit)
{
    std::cout << "mtp start\n";
    uint256 mtphash = mtp::hash(genesis, powLimit);
    std::cout << "mtp hash: " << mtphash.GetHex() << ", nonce: " << genesis.nNonce << '\n';
    std::cout << "mtp finish\n";
    return mtphash;
}

bool writeGenesisData(CBlock& block, const std::string& filename)
{
    // serialize block, checksum data up to that point, then append csum
    CDataStream ssdata(SER_DISK, CLIENT_VERSION);
    ssdata << block;

    // open temp output file, and associate with CAutoFile
    //boost::filesystem::path pathTmp = /*GetDataDir(false) /*/ filename;
    FILE *file = fopen(filename.c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s: Failed to open file %s", __func__, filename);
    uint256 hash = Hash(ssdata.begin(), ssdata.end());
    ssdata << hash;

    // Write and commit header, data
    try {
        fileout << ssdata;
    }
    catch (const std::exception& e) {
        return error("%s: Serialize or I/O error - %s", __func__, e.what());
    }
    FileCommit(fileout.Get());
    fileout.fclose();
    return true;
}

bool readGenesisData(CBlock& block, const std::string& filename)
{
    //boost::filesystem::path pathTmp = /*GetDataDir(false) /*/ filename;
    // open input file, and associate with CAutoFile
    FILE *file = fopen(filename.c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s: Failed to open file %s", __func__, filename);

    // use file size to size memory buffer
    uint64_t fileSize = boost::filesystem::file_size(filename);
    uint64_t dataSize = 0;
    // Don't try to resize to a negative number if file is small
    if (fileSize >= sizeof(uint256))
        dataSize = fileSize - sizeof(uint256);
    std::vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char *)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }
    filein.fclose();

    CDataStream ssdata(vchData, SER_DISK, CLIENT_VERSION);
    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssdata.begin(), ssdata.end());
    if (hashIn != hashTmp)
        return error("%s: Checksum mismatch, data corrupted", __func__);

    try {
        // de-serialize mtp data
        ssdata >> block;
    }
    catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }
    return true;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.BIP34Height = std::numeric_limits<int>::max();
        consensus.BIP34Hash = uint256S("0x01");
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.MMHeight = std::numeric_limits<int>::max();
        consensus.V7Height = std::numeric_limits<int>::max();
        consensus.powLimit = uint256S("0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~arith_uint256(0) >> 32;
        consensus.bnInitialHashTarget = uint256S("0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~arith_uint256(0) >> 32;
        consensus.nTargetTimespan = 24 * 60 * 60; // one week
        consensus.nTargetSpacing = 2 * 60;

        // mfcoin: PoS spacing = nStakeTargetSpacing
        //           PoW spacing = depends on how much PoS block are between last two PoW blocks, with maximum value = nTargetSpacingMax
        consensus.nStakeTargetSpacing = 2 * 60;                // 2 minutes
        consensus.nTargetSpacingMax = 12 * consensus.nStakeTargetSpacing; // 24 minutes
        consensus.nStakeMinAge = 60 * 60 * 24 * 14;             // minimum age for coin age
        consensus.nStakeMaxAge = 60 * 60 * 24 * 90;             // stake age of full weight
        consensus.nStakeModifierInterval = 60 * 60;         // time to elapse before new modifier is computed

        consensus.nCoinbaseMaturity = 80;
        consensus.nCoinbaseMaturityOld = 80;  // Used until block 193912 on mainNet.

        consensus.fPowAllowMinDifficultyBlocks = false;

        // The best chain should have at least this much work.
        consensus.nMinimumChainTrust = uint256S("0x00"); // at block 250 000

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00"); // at block 250 000

        consensus.nRejectBlockOutdatedMajority = 850;
        consensus.nToCheckBlockUpgradeMajority = 1000;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xf2;
        pchMessageStart[1] = 0xb6;
        pchMessageStart[2] = 0xf5;
        pchMessageStart[3] = 0xc3;
        vAlertPubKey = ParseHex("04e14603d29d0a051df1392c6256bb271ff4a7357260f8e2b82350ad29e1a5063d4a8118fa4cc8a0175cb45776e720cf4ef02cc2b160f5ef0144c3bb37ba3eea58");
        nDefaultPort = 22824;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1569921283, 1569921283, 5373, 0x1f0fffff, 1, 0);
        genesis.mtpHashValue = uint256S("0x000c2dcee2bd9d180c95f42a40006e3c6ca2db1c0a0863b6dd359940641ef999");
        // genesis.mtpHashValue = mineMtp(genesis, consensus.powLimit);
        // writeGenesisData(genesis, "genesis.dat");
        readGenesisData(genesis, (GetDefaultEnvPath() / "genesis.dat").string());
        consensus.hashGenesisBlock = genesis.GetHash();
        // printGenesisParams(genesis);
        assert(consensus.hashGenesisBlock == uint256S("0xe237705166fe5c4da57e166b22e4ddb37793d9bd416481dba0febae743f30706"));
        assert(genesis.hashMerkleRoot == uint256S("0xe8e2b49a9a97e28cb5a977cd588e36ca0ee73554164f9cfc337cff26fcbc1b70"));

        // Note that of those with the service bits flag, most only support a subset of possible options
        vSeeds.push_back(CDNSSeedData("mfcoin1", "n1.mfcoin.net"));
        vSeeds.push_back(CDNSSeedData("mfcoin2", "n2.mfcoin.net"));
        vSeeds.push_back(CDNSSeedData("mfcoin3", "n3.mfcoin.net"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,51);   // mfcoin: addresses begin with 'M'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);   // mfcoin: addresses begin with 'e'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,179);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x76)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x76)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of(0, uint256S("0xe237705166fe5c4da57e166b22e4ddb37793d9bd416481dba0febae743f30706"))};

        chainTxData = ChainTxData{
            // Data as of block ???
            0, // * UNIX timestamp of last known number of transactions
            0,     // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0 // * estimated number of transactions per second after that timestamp
        };
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.BIP34Height = std::numeric_limits<int>::max();
        consensus.BIP34Hash = uint256S("0x01");
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.MMHeight = std::numeric_limits<int>::max();
        consensus.V7Height = std::numeric_limits<int>::max();
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~arith_uint256(0) >> 28;
        consensus.bnInitialHashTarget = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); //~uint256(0) >> 29;
        consensus.nTargetTimespan = 12 * 60 * 60; //7 * 24 * 60 * 60; // two week
        consensus.nTargetSpacing = 2 * 60;

        // mfcoin: PoS spacing = nStakeTargetSpacing
        //           PoW spacing = depends on how much PoS block are between last two PoW blocks, with maximum value = nTargetSpacingMax
        consensus.nStakeTargetSpacing = 2 * 60;                // 2 minutes
        consensus.nTargetSpacingMax = 12 * consensus.nStakeTargetSpacing; // 24 minutes
        consensus.nStakeMinAge = 60;                  // minimum age for coin age
        consensus.nStakeMaxAge = 60 * 60 * 24 * 1;             // stake age of full weight
        consensus.nStakeModifierInterval = 60 * 20;              // time to elapse before new modifier is computed

        consensus.nCoinbaseMaturity = 12;
        consensus.nCoinbaseMaturityOld = 12;

        consensus.fPowAllowMinDifficultyBlocks = true;

        // The best chain should have at least this much work.
        consensus.defaultAssumeValid = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        consensus.nRejectBlockOutdatedMajority = 450;
        consensus.nToCheckBlockUpgradeMajority = 500;

        pchMessageStart[0] = 0xc4;
        pchMessageStart[1] = 0xd8;
        pchMessageStart[2] = 0xe2;
        pchMessageStart[3] = 0xc1;
        nDefaultPort = 33813;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1560542398, 1560542398, 1, 0x207fffff, 1, 0);
        //std::cout << "testnet\n";
        //mineMtp(genesis, consensus.powLimit);
        genesis.mtpHashValue = uint256S("0x7d33e93c826a8b7ea2f7c5fb14677a440994844ae2df1faad93af04f63c84b54");
        //writeGenesisData(genesis, "genesis-test.dat");
        readGenesisData(genesis, (GetDefaultEnvPath() / "genesis-test.dat").string());
        //printGenesisParams(genesis, true);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0xb98c3956a2fc4ad9027cd8858dc32fba36f5bfdb01e64856879d393f34297013"));
        assert(genesis.hashMerkleRoot == uint256S("0x683ce125249fac7ada1947d926a10f4775b680c4ed3e957fc83ad042dffe795a"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        //vSeeds.push_back(CDNSSeedData("mfcoin", "tnseed.mfcoin.net"));

        //base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);     // Testnet pubkey hash: m or n
        //base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,198);     // Testnet script hash: 2
        //base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,242);
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,51);     // Testnet pubkey hash: m or n
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);     // Testnet script hash: 2
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,179);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            ( 0,  uint256S("0xb98c3956a2fc4ad9027cd8858dc32fba36f5bfdb01e64856879d393f34297013"))
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 0; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 0; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.MMHeight = 0;
        consensus.V7Height = 457;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.bnInitialHashTarget = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); //~uint256(0) >> 29;
        consensus.nTargetTimespan = 7 * 24 * 60 * 60; // one week
        consensus.nTargetSpacing = 10 * 60;

        // mfcoin: PoS spacing = nStakeTargetSpacing
        //           PoW spacing = depends on how much PoS block are between last two PoW blocks, with maximum value = nTargetSpacingMax
        consensus.nStakeTargetSpacing = 10 * 60;                // 10 minutes
        consensus.nTargetSpacingMax = 12 * consensus.nStakeTargetSpacing; // 2 hours
        consensus.nStakeMinAge = 60 * 60 * 24;                  // minimum age for coin age
        consensus.nStakeMaxAge = 60 * 60 * 24 * 90;             // stake age of full weight
        consensus.nStakeModifierInterval = 6 * 20;              // time to elapse before new modifier is computed

        consensus.nCoinbaseMaturity = 32;
        consensus.nCoinbaseMaturityOld = 32;

        consensus.fPowAllowMinDifficultyBlocks = true;

        // The best chain should have at least this much work.
        consensus.nMinimumChainTrust = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        consensus.nRejectBlockOutdatedMajority = 850;
        consensus.nToCheckBlockUpgradeMajority = 1000;

        pchMessageStart[0] = 0xcb;
        pchMessageStart[1] = 0xf2;
        pchMessageStart[2] = 0xc0;
        pchMessageStart[3] = 0xef;
        nDefaultPort = 22826;
        nPruneAfterHeight = 1000;

        //std::cout << "regtestnet\n";
        genesis = CreateGenesisBlock(1386627289, 1386628033, 0, 0x207fffff, 1, 0);
        //printGenesisParams(genesis, true);
        //mineMtp(genesis, consensus.powLimit);
        genesis.mtpHashValue = uint256S("0x3ec0a8bf848217723029c2df2a7d3e5ed1fac7d4f30f52b604759ec7a5d479fa");
        readGenesisData(genesis, (GetDefaultEnvPath() / "genesis-reg.dat").string());
        //printGenesisParams(genesis, true);
        //std::cout << "!!\n" << genesis.mtpHashData->toString() << "!!\n";
        consensus.hashGenesisBlock = genesis.GetHash();
        //assert(consensus.hashGenesisBlock == uint256S("0xd562b71f43864137dd4a64559e876dd3a081d241723559a685fa033603992872"));
        assert(genesis.hashMerkleRoot == uint256S("0xd8eee032f95716d0cf14231dc7a238b96bbf827e349e75344c9a88e849262ee0"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0x0000000642cfda7d39a8281e1f8791ceb240ce2f5ed9082f60040fe4210c6a58"))
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
    }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
            return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
            return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
            return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}
