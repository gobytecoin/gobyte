// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <random.h>
#include <util.h>
#include <test/test_gobyte.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

/* Test calculation of next difficulty target with DGW */
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

    // build the chain of 24 blocks
    CBlockIndex blockIndexLast;
    blockIndexLast.nHeight = 123456;
    blockIndexLast.nTime = 1530377693;
    blockIndexLast.nBits = 0x1c009da0;
    CBlockIndex blockIndexPrev1 = CBlockIndex();
    blockIndexPrev1.nTime = 1530377481;  // Block #123455
    blockIndexPrev1.nBits = 0x1c009c4b;
    blockIndexLast.pprev = &blockIndexPrev1;
    CBlockIndex blockIndexPrev2 = CBlockIndex();
    blockIndexPrev2.nTime = 1530377390;  // Block #123454
    blockIndexPrev2.nBits = 0x1c00a00c;
    blockIndexPrev1.pprev = &blockIndexPrev2;
    CBlockIndex blockIndexPrev3 = CBlockIndex();
    blockIndexPrev3.nTime = 1530377366;  // Block #123453
    blockIndexPrev3.nBits = 0x1b739a9e;
    blockIndexPrev2.pprev = &blockIndexPrev3;
    CBlockIndex blockIndexPrev4 = CBlockIndex();
    blockIndexPrev4.nTime = 1530375925;  // Block #123452
    blockIndexPrev4.nBits = 0x1b77e92f;
    blockIndexPrev3.pprev = &blockIndexPrev4;
    CBlockIndex blockIndexPrev5 = CBlockIndex();
    blockIndexPrev5.nTime = 1530375873;  // Block #123451
    blockIndexPrev5.nBits = 0x1b7a3220;
    blockIndexPrev4.pprev = &blockIndexPrev5;
    CBlockIndex blockIndexPrev6 = CBlockIndex();
    blockIndexPrev6.nTime = 1530375870;  // Block #123450
    blockIndexPrev6.nBits = 0x1c008bcd;
    blockIndexPrev5.pprev = &blockIndexPrev6;
    CBlockIndex blockIndexPrev7 = CBlockIndex();
    blockIndexPrev7.nTime = 1530375865;  // Block #123449
    blockIndexPrev7.nBits = 0x1c008dcd;
    blockIndexPrev6.pprev = &blockIndexPrev7;
    CBlockIndex blockIndexPrev8 = CBlockIndex();
    blockIndexPrev8.nTime = 1530375861;  // Block #123448
    blockIndexPrev8.nBits = 0x1c008d8a;
    blockIndexPrev7.pprev = &blockIndexPrev8;
    CBlockIndex blockIndexPrev9 = CBlockIndex();
    blockIndexPrev9.nTime = 1530375859;  // Block #123447
    blockIndexPrev9.nBits = 0x1c008d28;
    blockIndexPrev8.pprev = &blockIndexPrev9;
    CBlockIndex blockIndexPrev10 = CBlockIndex();
    blockIndexPrev10.nTime = 1530375830;  // Block #123446
    blockIndexPrev10.nBits = 0x1c008d53;
    blockIndexPrev9.pprev = &blockIndexPrev10;
    CBlockIndex blockIndexPrev11 = CBlockIndex();
    blockIndexPrev11.nTime = 1530375827;  // Block #123445
    blockIndexPrev11.nBits = 0x1c008b65;
    blockIndexPrev10.pprev = &blockIndexPrev11;
    CBlockIndex blockIndexPrev12 = CBlockIndex();
    blockIndexPrev12.nTime = 1530375785;  // Block #123444
    blockIndexPrev12.nBits = 0x1b68276f;
    blockIndexPrev11.pprev = &blockIndexPrev12;
    CBlockIndex blockIndexPrev13 = CBlockIndex();
    blockIndexPrev13.nTime = 1530374794;  // Block #123443
    blockIndexPrev13.nBits = 0x1b588ba1;
    blockIndexPrev12.pprev = &blockIndexPrev13;
    CBlockIndex blockIndexPrev14 = CBlockIndex();
    blockIndexPrev14.nTime = 1530374308;  // Block #123442
    blockIndexPrev14.nBits = 0x1b576483;
    blockIndexPrev13.pprev = &blockIndexPrev14;
    CBlockIndex blockIndexPrev15 = CBlockIndex();
    blockIndexPrev15.nTime = 1530374205;  // Block #123441
    blockIndexPrev15.nBits = 0x1c00931c;
    blockIndexPrev14.pprev = &blockIndexPrev15;
    CBlockIndex blockIndexPrev16 = CBlockIndex();
    blockIndexPrev16.nTime = 1530374114;  // Block #123440
    blockIndexPrev16.nBits = 0x1c009020;
    blockIndexPrev15.pprev = &blockIndexPrev16;
    CBlockIndex blockIndexPrev17 = CBlockIndex();
    blockIndexPrev17.nTime = 1530374002;  // Block #123439
    blockIndexPrev17.nBits = 0x1c00868e;
    blockIndexPrev16.pprev = &blockIndexPrev17;
    CBlockIndex blockIndexPrev18 = CBlockIndex();
    blockIndexPrev18.nTime = 1530373752;  // Block #123438
    blockIndexPrev18.nBits = 0x1c008496;
    blockIndexPrev17.pprev = &blockIndexPrev18;
    CBlockIndex blockIndexPrev19 = CBlockIndex();
    blockIndexPrev19.nTime = 1530373666;  // Block #123437
    blockIndexPrev19.nBits = 0x1c008369;
    blockIndexPrev18.pprev = &blockIndexPrev19;
    CBlockIndex blockIndexPrev20 = CBlockIndex();
    blockIndexPrev20.nTime = 1530373598;  // Block #123436
    blockIndexPrev20.nBits = 0x1c008a48;
    blockIndexPrev19.pprev = &blockIndexPrev20;
    CBlockIndex blockIndexPrev21 = CBlockIndex();
    blockIndexPrev21.nTime = 1530373566;  // Block #123435
    blockIndexPrev21.nBits = 0x1c008982;
    blockIndexPrev20.pprev = &blockIndexPrev21;
    CBlockIndex blockIndexPrev22 = CBlockIndex();
    blockIndexPrev22.nTime = 1530373343;  // Block #123434
    blockIndexPrev22.nBits = 0x1c00909f;
    blockIndexPrev21.pprev = &blockIndexPrev22;
    CBlockIndex blockIndexPrev23 = CBlockIndex();
    blockIndexPrev23.nTime = 1530373334;  // Block #123433
    blockIndexPrev23.nBits = 0x1c0094ce;
    blockIndexPrev22.pprev = &blockIndexPrev23;

    CBlockHeader blockHeader;
    blockHeader.nTime = 1530377830; // Block #123457
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blockIndexLast, &blockHeader, chainParams->GetConsensus()), 0x1c00a362); // Block #123457 has 0x1c00a362

    // test special rules for slow blocks on devnet/testnet
    gArgs.SoftSetBoolArg("-devnet", true);
    const auto chainParamsDev = CreateChainParams(CBaseChainParams::DEVNET);

    // make sure normal rules apply
    blockHeader.nTime = 1564741609; // Block #123457
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blockIndexLast, &blockHeader, chainParamsDev->GetConsensus()), 0x1c00a362); // Block #123457 has 0x1b1441de

    // 10x higher target
    blockHeader.nTime = 1564742210; // Block #123457 (10m+1sec)
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blockIndexLast, &blockHeader, chainParamsDev->GetConsensus()), 0x1c00a362); // Block #123457 has 0x1c00c8f8
    blockHeader.nTime = 1564742809; // Block #123457 (20m)
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blockIndexLast, &blockHeader, chainParamsDev->GetConsensus()), 0x1c00a362); // Block #123457 has 0x1c00c8f8
    // lowest diff possible
    blockHeader.nTime = 1564748810; // Block #123457 (2h+1sec)
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blockIndexLast, &blockHeader, chainParamsDev->GetConsensus()), 0x1c00a362); // Block #123457 has 0x207fffff
    blockHeader.nTime = 1564752409; // Block #123457 (3h)
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blockIndexLast, &blockHeader, chainParamsDev->GetConsensus()), 0x1c00a362); // Block #123457 has 0x207fffff
}

/* Test the constraint on the upper bound for next work */
// BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
// {
//     const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

//     int64_t nLastRetargetTime = 1231006505; // Block #0
//     CBlockIndex pindexLast;
//     pindexLast.nHeight = 2015;
//     pindexLast.nTime = 1233061996;  // Block #2015
//     pindexLast.nBits = 0x1d00ffff;
//     BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1d00ffff);
// }

/* Test the constraint on the lower bound for actual time taken */
// BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
// {
//     const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

//     int64_t nLastRetargetTime = 1279008237; // Block #66528
//     CBlockIndex pindexLast;
//     pindexLast.nHeight = 68543;
//     pindexLast.nTime = 1279297671;  // Block #68543
//     pindexLast.nBits = 0x1c05a3f4;
//     BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1c0168fd);
// }

/* Test the constraint on the upper bound for actual time taken */
// BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
// {
//     const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

//     int64_t nLastRetargetTime = 1263163443; // NOTE: Not an actual block time
//     CBlockIndex pindexLast;
//     pindexLast.nHeight = 46367;
//     pindexLast.nTime = 1269211443;  // Block #46367
//     pindexLast.nBits = 0x1c387f6f;
//     BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1d00e1fd);
// }

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p2 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p3 = &blocks[InsecureRandRange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

BOOST_AUTO_TEST_SUITE_END()
