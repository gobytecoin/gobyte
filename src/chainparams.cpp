// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2017-2019 The GoByte Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include "chainparamsseeds.h"

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
 *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
 *   vMerkleTree: e0028e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The Star Malaysia 17th November 2017 GoByte Genesis Reborn";
    const CScript genesisOutputScript = CScript() << ParseHex("043e5a5fbfbb2caa5f4b7c8fd24d890d6c244de254d579b5ba629f64c1b48275f59e0e1c834a60f6ffb4aaa022aaa4866434ca729a12465f80618fb2070045cb16") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
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
        consensus.nSubsidyHalvingInterval = 210240; // one year
        consensus.nMasternodePaymentsStartBlock = 1152; // not true, but it's ok as long as it's less then nMasternodePaymentsIncreaseBlock
        consensus.nMasternodePaymentsIncreaseBlock = 124000; // actual historical value
        consensus.nMasternodePaymentsIncreasePeriod = 576*30; // 17280 - actual historical value
        consensus.nInstantSendKeepLock = 24;
        consensus.nBudgetPaymentsStartBlock = 158560;
        consensus.nBudgetPaymentsCycleBlocks = 17280; // ~(60*24*30)/2.5 = 1 Month
        consensus.nBudgetPaymentsWindowBlocks = 100;
        consensus.nBudgetProposalEstablishingTime = 60*60*24;
        consensus.nSuperblockStartBlock = 162592;
        consensus.nSuperblockCycle = 17280; // ~(60*24*30)/2.5
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nMasternodeMinimumConfirmations = 15;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("0x00000c8a1ff01bae3f3875c81cb14115429af5744643b34b4ad1cbb7d2d59ca2");
        consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000");
        consensus.nPowTargetTimespan = 60 * 60; // GoByte: 1 hour, 24 blocks
        consensus.nPowTargetSpacing = 2.5 * 60; // GoByte: 150 seconds
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nPowDGWHeight = 700;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1502280000; // Aug 9th, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1533816000; // Aug 9th, 2018

        // Deployment of DIP0001
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 1525104000; // May 1st, 2018
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = 1556640000; // May 1st, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 4032;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 3226; // 80% of 4032

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000000000004ce57668f63f9b4"); // 85000

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000000000465ca188f7524071debf90b7fa9716736673a8641a82805119b2f0"); // 85000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x1a;
        pchMessageStart[1] = 0xb2;
        pchMessageStart[2] = 0xc3;
        pchMessageStart[3] = 0xd4;
        vAlertPubKey = ParseHex("045f6f7880946beb809b89610a5c1fa3a25604e6a1fe0642576390f3c3be43f7d175cd3e62cbe90fac8868a3ba25ebcefb382f3ddaf0de20ca3a2697ed21b75110");
        nDefaultPort = 12455;
        nMaxTipAge = 6 * 60 * 60; // ~144 blocks behind -> 2 x fork detection time, was 24 * 60 * 60 in bitcoin
        nDelayGetHeadersTime = 24 * 60 * 60;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1510848000, 1631855, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0000033b01055cf8df90b01a14734cae92f7039b9b0e48887b4e33a469d7bc07"));
        assert(genesis.hashMerkleRoot == uint256S("0xdc9a719dc1bcda39107ea55424f00cab512170a1cb69efa08531f483f2399f21"));

        vSeeds.push_back(CDNSSeedData("seed1.gobyte.network", "seed1.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed2.gobyte.network", "seed2.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed3.gobyte.network", "seed3.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed4.gobyte.network", "seed4.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed5.gobyte.network", "seed5.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed6.gobyte.network", "seed6.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed7.gobyte.network", "seed7.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed8.gobyte.network", "seed8.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed9.gobyte.network", "seed9.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("seed10.gobyte.network", "seed10.gobyte.network"));

        // GoByte addresses start with 'G'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,38);
        // GoByte script addresses start with '5'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,10);
        // GoByte private keys start with '5' or 'G' (?)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,198);
        // GoByte BIP32 pubkeys start with 'xpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        // GoByte BIP32 prvkeys start with 'xprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();

        // GoByte BIP44 coin type is '176'
        nExtCoinType = 176;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 60*60; // fulfilled requests expire in 1 hour
        strSporkPubKey = "0436e3a43b74cb522c19cd0c2c8073152b60c227f1360c7d3249f995cc19466ee132d3843fcb3379f10c0ea2336112f1283dc78c04df5c7bd39c8896627ac60d4c";

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (   500, uint256S("0x00000000009d382b57dda9a0e169d1f3f6f384eff5aecbf24329457bb1b30962"))
            (  5500, uint256S("0x00000000002e3468a5196451755b5ff8d27cb7260a34250c98b0607eddfd91b2"))
            ( 10500, uint256S("0x000000000018df60822b6fc7e13b2da59e5c7ee29e015bebb7cfaddc22001180"))
            ( 15500, uint256S("0x00000000003c1ae6c074c7798f1b8111916bfa86a7f41ec51633d1fc1c925140"))
            ( 20500, uint256S("0x0000000000702ff7699e8efd6af8b0ed888f77152078ed7bc782ece100e134aa"))
            ( 25500, uint256S("0x00000000002e31e50a1753f1eff7b9d6e34b4a9def0aecd232a30b8cd5a602b4"))
            ( 30500, uint256S("0x000000000043b2ab4bd00554e3265d00d89144ac38fe309b23030416f00bf977"))
            ( 35500, uint256S("0x000000000005e20fc8021a844309e6d3e9835cd85245584a163379be39d8905f"))
            ( 40500, uint256S("0x0000000000289c1d313b29dd50d86cbe6be0bf820c003a48244d0ec1e90e2849"))
            ( 45500, uint256S("0x000000000000e3567b64c9628340e5cee8fc339b775c6164dd0333943faff781"))
            ( 50500, uint256S("0x0000000000266c5869756c89df175674adafae97b26dddc5e77e9a93352cb5bc"))
            ( 55500, uint256S("0x000000000005515ab76f79bde997aa9843efb378dd25dfad156a6ca7d5d15f69"))
            ( 60500, uint256S("0x00000000003d534ac8baffd1ed8c9f2ea5481f22f8796643210d6dfeb70def28"))
            ( 65500, uint256S("0x000000000026bbdcf9b3267bb030fc31a2b9c6eb6cd1f9b3694c0df8bf2c67a4"))
            ( 70500, uint256S("0x0000000000108504d132a342a87e6c0afaf75737e60b43e7b446a4d7b5716de5"))
            ( 75500, uint256S("0x000000000023159c0785a3eb1b1541d25347f649f149a3f53c32579364d6482a"))
            ( 80500, uint256S("0x000000000001b919061af98801d30b9351226a555e1f265c9cd880b18e6ec405"))
            ( 85500, uint256S("0x00000000003ead42a3ba43e9c0a536b6aef038aea4d6b3961df2af6edec26106"))
            ( 90500, uint256S("0x000000000030eff4e52d37edee0dc14c13168fee49c743e32fcec206003844e0"))
            ( 95500, uint256S("0x000000000078e57c0dc90b7dd4027ea54279c63c62bb6b7a38349ec783613056"))
            ( 100500, uint256S("0x00000000004f79e57507c1b26b962f25f5102d43cfab93de118346b4a0426626"))
            ( 105500, uint256S("0x000000000023363fc795f4cb55d87f2b17f5d0353a5a65402f51057543c764af"))
            ( 110500, uint256S("0x000000000028fe8242eb23e939c7e6b2063d5a4cea533402e35a3c6557d02bc6"))
            ( 115500, uint256S("0x000000000074763fd76ef4a344d4f7e049c1dc55395c3814ac608b6fde6c7ecb"))
            ( 115890, uint256S("0x0000000000a8de066324ed2575d7e2b5ba21bc8eeaaa5dae2c37d6177953ac3c"))
            ( 234000, uint256S("0x00000000006bc1e30c637f0ed0d573d90e63e71f0baedac29af78e6311007e3d"))
            ( 556750, uint256S("0x0000000003174913922493cb828474c666b634bd169a7221c9387528a69ec957")),

            1598853165, // * UNIX timestamp of last checkpoint block
            970293,   // * total number of transactions between genesis and last checkpoint
                        //   (the tx=... number in the SetBestChain debug.log lines)
            2800        // * estimated number of transactions per day after checkpoint
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
        consensus.nSubsidyHalvingInterval = 210240;
        consensus.nMasternodePaymentsStartBlock = 4010; // not true, but it's ok as long as it's less then nMasternodePaymentsIncreaseBlock
        consensus.nMasternodePaymentsIncreaseBlock = 4030;
        consensus.nMasternodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = 4100;
        consensus.nBudgetPaymentsCycleBlocks = 50; // ~(60*24*30)/2.5
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nBudgetProposalEstablishingTime = 60*20;
        consensus.nSuperblockStartBlock = 4200; // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPeymentsStartBlock
        consensus.nSuperblockCycle = 24; // Superblocks can be issued hourly on testnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 100;
        consensus.BIP34Height = 50;
        consensus.BIP34Hash = uint256S("0x0000039db8b789064e2370998cdd0a914cf516869796fe5da5ae095a94eaa812");
        consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000");
        consensus.nPowTargetTimespan = 60 * 60; // GoByte: 1 hour, 24 blocks
        consensus.nPowTargetSpacing = 2.5 * 60; // GoByte: 150 seconds
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nPowDGWHeight = 650;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1622288970; // May 29, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1653824970; // May 29, 2022

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1622288970; // May 29, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1653824970; // May 29, 2022

        // Deployment of DIP0001
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 1622288970; // May 29, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = 1653824970; // May 29, 2022
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 50; // 50% of 100

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000009e3e9dbc"); // 1359 getblockheader

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x000004cc71734bb41970f884c9c5a6bcced9dba542b900c818d9be964c3b4a0f"); // 1359 getblockhash

        pchMessageStart[0] = 0xd1;
        pchMessageStart[1] = 0x2b;
        pchMessageStart[2] = 0xb3;
        pchMessageStart[3] = 0x7a;
        vAlertPubKey = ParseHex("0458b688c478d2a3febf4e86db1b1c2f862cfb31780b6247c7caa35145e9cb16f55a60cc476654fedaa1dcd807c8432ee5a5f2324f727a0531713c66f7734b351c");
        nDefaultPort = 13455;
        nMaxTipAge = 0x7fffffff; // allow mining on top of old blocks for testnet
        nDelayGetHeadersTime = 24 * 60 * 60;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1621860500, 1181433, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000a63181b01cd202a3e28380b334d107f01cbc0a831c2ab0751bb3370fe7a"));
        assert(genesis.hashMerkleRoot == uint256S("0xdc9a719dc1bcda39107ea55424f00cab512170a1cb69efa08531f483f2399f21"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("gobyte.network",  "testnet-dns.gobyte.network"));
        vSeeds.push_back(CDNSSeedData("gobyte.network",  "testnet2-dns.gobyte.network"));

        // Testnet GoByte addresses start with 'n'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,112);
        // Testnet GoByte script addresses start with '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,20);
        // Testnet private keys start with '9' or 'c' (Bitcoin defaults) (?)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,240);
        // Testnet GoByte BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Testnet GoByte BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        // Testnet GoByte BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        strSporkPubKey = "0488ee83bcfcfaff3b270d8b23ce7956f0e4f2ae710941f6029a1eaf04169186e586e2b0e4d0b25580e729264c76b5d7da73f3bb3a7641ba1c114268d3d8e68d9c";

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (  250, uint256S("0x000002b73b3acc90c891363127fc666b0d19e899a19d4d17d674d9358ba3f604"))
            (  999, uint256S("0x000000463cc00b83ca1c6448e1659b588a454c34f3334c5f50c94b16c4257239"))
            (  1299, uint256S("0x00000591a751dab5204cfb18fcba053a8307159f4d0bad035c0a11e1bcb65749")),
            1622290075, // * UNIX timestamp of last checkpoint block
            1367,       // * total number of transactions between genesis and last checkpoint
                        //   (the tx=... number in the SetBestChain debug.log lines)
            100         // * estimated number of transactions per day after checkpoint
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
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nMasternodePaymentsStartBlock = 240;
        consensus.nMasternodePaymentsIncreaseBlock = 350;
        consensus.nMasternodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = 1000;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nBudgetProposalEstablishingTime = 60*20;
        consensus.nSuperblockStartBlock = 1500;
        consensus.nSuperblockCycle = 10;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.BIP34Height = -1; // BIP34 has not necessarily activated on regtest
        consensus.BIP34Hash = uint256();
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 60 * 60; // GoByte: 1 hour, 24 blocks
        consensus.nPowTargetSpacing = 2.5 * 60; // GoByte: 150 seconds
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nPowDGWHeight = 700; // same as mainnet
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xa1;
        pchMessageStart[1] = 0xb3;
        pchMessageStart[2] = 0xd5;
        pchMessageStart[3] = 0x7b;
        nMaxTipAge = 6 * 60 * 60; // ~144 blocks behind -> 2 x fork detection time, was 24 * 60 * 60 in bitcoin
        nDelayGetHeadersTime = 0; // never delay GETHEADERS in regtests
        nDefaultPort = 13565;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1510727100, 901219, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000dbc9aa1686b4dfb177300185c6a3e0b13d1d4d346c5bccdd19fdf9ebc5a"));
        assert(genesis.hashMerkleRoot == uint256S("0xdc9a719dc1bcda39107ea55424f00cab512170a1cb69efa08531f483f2399f21"));

        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0x00000dbc9aa1686b4dfb177300185c6a3e0b13d1d4d346c5bccdd19fdf9ebc5a")),
            0,
            0,
            0
        };
        // Regtest GoByte addresses start with 'n'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,112);
        // Regtest GoByte script addresses start with '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,20);
        // Regtest private keys start with '9' or 'c' (Bitcoin defaults) (?)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,240);
        // Regtest GoByte BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Regtest GoByte BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        // Regtest GoByte BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;
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
