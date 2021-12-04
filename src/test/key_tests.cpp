// Copyright (c) 2012-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>

#include <base58.h>
#include <script/script.h>
#include <uint256.h>
#include <util.h>
#include <utilstrencodings.h>
#include <test/test_gobyte.h>

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

static const std::string strSecret1 = "7eFKZu9ukdb8RvdyXg4URe118AkwzCD2fxdiTM7ffpvECydN3PR";
static const std::string strSecret2 = "7fPCBtnCVK1vwmjqAnBR96N1Mt4RncysjZEWNBNg3gmV3a6DtCQ";
static const std::string strSecret1C = "WKQCxDCKqaVdo1kCWPQL3kK1Ye4yhSU6NxZ3yHnRgNVwHQ3XsbX5";
static const std::string strSecret2C = "WQQxP5YVhxX76kU3cAQyYifibAN8QMp2vqGccUnfLrXLmzcyE9MW";
static const std::string addr1 = "GTXBtrVsZgme3LqCG2BtFf2wjdHrcTM6Sq";
static const std::string addr2 = "Gf5qkGLd8L5ApFvZtjfGKHckoFi8WzrX7S";
static const std::string addr1C = "GYQB3b2EhTHQcPT4SmnFmE8Jmc1a7uTNg1";
static const std::string addr2C = "GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb";

static const std::string strAddressBad = "Gdfa28We1W24J1uMvQeC6km5JAD9sw8gPe";


BOOST_FIXTURE_TEST_SUITE(key_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(key_test1)
{
    CBitcoinSecret bsecret1, bsecret2, bsecret1C, bsecret2C, baddress1;
    BOOST_CHECK( bsecret1.SetString (strSecret1));
    BOOST_CHECK( bsecret2.SetString (strSecret2));
    BOOST_CHECK( bsecret1C.SetString(strSecret1C));
    BOOST_CHECK( bsecret2C.SetString(strSecret2C));
    BOOST_CHECK(!baddress1.SetString(strAddressBad));

    CKey key1  = bsecret1.GetKey();
    BOOST_CHECK(key1.IsCompressed() == false);
    CKey key2  = bsecret2.GetKey();
    BOOST_CHECK(key2.IsCompressed() == false);
    CKey key1C = bsecret1C.GetKey();
    BOOST_CHECK(key1C.IsCompressed() == true);
    CKey key2C = bsecret2C.GetKey();
    BOOST_CHECK(key2C.IsCompressed() == true);

    CPubKey pubkey1  = key1. GetPubKey();
    CPubKey pubkey2  = key2. GetPubKey();
    CPubKey pubkey1C = key1C.GetPubKey();
    CPubKey pubkey2C = key2C.GetPubKey();

    BOOST_CHECK(key1.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key1C.VerifyPubKey(pubkey1));
    BOOST_CHECK(key1C.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key1C.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key1C.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key2.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key2.VerifyPubKey(pubkey1C));
    BOOST_CHECK(key2.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key2.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key2C.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key2C.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key2C.VerifyPubKey(pubkey2));
    BOOST_CHECK(key2C.VerifyPubKey(pubkey2C));

    BOOST_CHECK(DecodeDestination(addr1)  == CTxDestination(pubkey1.GetID()));
    BOOST_CHECK(DecodeDestination(addr2)  == CTxDestination(pubkey2.GetID()));
    BOOST_CHECK(DecodeDestination(addr1C) == CTxDestination(pubkey1C.GetID()));
    BOOST_CHECK(DecodeDestination(addr2C) == CTxDestination(pubkey2C.GetID()));

    for (int n=0; n<16; n++)
    {
        std::string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());

        // normal signatures

        std::vector<unsigned char> sign1, sign2, sign1C, sign2C;

        BOOST_CHECK(key1.Sign (hashMsg, sign1));
        BOOST_CHECK(key2.Sign (hashMsg, sign2));
        BOOST_CHECK(key1C.Sign(hashMsg, sign1C));
        BOOST_CHECK(key2C.Sign(hashMsg, sign2C));

        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2C));

        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2C));

        // compact signatures (with key recovery)

        std::vector<unsigned char> csign1, csign2, csign1C, csign2C;

        BOOST_CHECK(key1.SignCompact (hashMsg, csign1));
        BOOST_CHECK(key2.SignCompact (hashMsg, csign2));
        BOOST_CHECK(key1C.SignCompact(hashMsg, csign1C));
        BOOST_CHECK(key2C.SignCompact(hashMsg, csign2C));

        CPubKey rkey1, rkey2, rkey1C, rkey2C;

        BOOST_CHECK(rkey1.RecoverCompact (hashMsg, csign1));
        BOOST_CHECK(rkey2.RecoverCompact (hashMsg, csign2));
        BOOST_CHECK(rkey1C.RecoverCompact(hashMsg, csign1C));
        BOOST_CHECK(rkey2C.RecoverCompact(hashMsg, csign2C));

        BOOST_CHECK(rkey1  == pubkey1);
        BOOST_CHECK(rkey2  == pubkey2);
        BOOST_CHECK(rkey1C == pubkey1C);
        BOOST_CHECK(rkey2C == pubkey2C);
    }

    // test deterministic signing

    std::vector<unsigned char> detsig, detsigc;
    std::string strMsg = "Very deterministic message";
    uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());
    BOOST_CHECK(key1.Sign(hashMsg, detsig));
    BOOST_CHECK(key1C.Sign(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig == ParseHex("304402203977ef3cc6015367cd1daa7dc15aa7462bcba3b50925536b7d09728bb7fdcb0402201f07234109bc7ac1aa3a1ec7aea95b3804532e1159aaccecc17665382d68a1b8"));
    BOOST_CHECK(key2.Sign(hashMsg, detsig));
    BOOST_CHECK(key2C.Sign(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig == ParseHex("3044022061890d89832b93cbcb53e363f1c9bc86b84f5c78bbe94b25b7ae6e6653e03cf6022043a4670d2c5f971200a9b37f0f9e0a3eadbe062117719fb7713fbcc9684a227f"));
    BOOST_CHECK(key1.SignCompact(hashMsg, detsig));
    BOOST_CHECK(key1C.SignCompact(hashMsg, detsigc));
    BOOST_CHECK(detsig == ParseHex("1b3977ef3cc6015367cd1daa7dc15aa7462bcba3b50925536b7d09728bb7fdcb041f07234109bc7ac1aa3a1ec7aea95b3804532e1159aaccecc17665382d68a1b8"));
    BOOST_CHECK(detsigc == ParseHex("1f3977ef3cc6015367cd1daa7dc15aa7462bcba3b50925536b7d09728bb7fdcb041f07234109bc7ac1aa3a1ec7aea95b3804532e1159aaccecc17665382d68a1b8"));
    BOOST_CHECK(key2.SignCompact(hashMsg, detsig));
    BOOST_CHECK(key2C.SignCompact(hashMsg, detsigc));
    BOOST_CHECK(detsig == ParseHex("1c61890d89832b93cbcb53e363f1c9bc86b84f5c78bbe94b25b7ae6e6653e03cf643a4670d2c5f971200a9b37f0f9e0a3eadbe062117719fb7713fbcc9684a227f"));
    BOOST_CHECK(detsigc == ParseHex("2061890d89832b93cbcb53e363f1c9bc86b84f5c78bbe94b25b7ae6e6653e03cf643a4670d2c5f971200a9b37f0f9e0a3eadbe062117719fb7713fbcc9684a227f"));
}

BOOST_AUTO_TEST_SUITE_END()
