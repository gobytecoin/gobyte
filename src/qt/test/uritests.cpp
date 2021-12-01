// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/uritests.h>

#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?label=Some Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(rv.label == QString("Some Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?amount=100&label=Some Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Some Example"));

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?message=Some Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("gobyte://GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?message=Some Example Address", &rv));
    QVERIFY(rv.address == QString("GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?req-message=Some Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?amount=1,000&label=Some Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?amount=1,000.0&label=Some Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?amount=100&label=Some Example&message=Some Example Message"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Some Example"));
    QVERIFY(rv.message == QString("Some Example Message"));

    // Verify that IS=xxx does not lead to an error (we ignore the field)
    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?IS=1"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb?req-IS=1"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("gobyte:GRDScwm14buqH1Wv1K6SMwNKGUHa4Jt2nb"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
}
