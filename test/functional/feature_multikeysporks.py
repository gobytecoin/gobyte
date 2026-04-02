#!/usr/bin/env python3
# Copyright (c) 2018-2021 The GoByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import connect_nodes, wait_until

'''
feature_multikeysporks.py

Test logic for several signer keys usage for spork broadcast.

We set 5 possible keys for sporks signing and set minimum
required signers to 3. We check 1 and 2 signers can't set the spork
value, any 3 signers can change spork value and other 3 signers
can change it again.
'''


class MultiKeySporkTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 5
        self.setup_clean_chain = True
        self.is_network_split = False

    def setup_network(self):
        # secret(base58): cXfsWQHV7koi4ScASoNMRFA3pGqC4Yp39BsSezRGA34FbF3biABP
        # address(base58): nDQMa7iiKMXnZoU3CXaXs7VXvugYCjFsPn

        # secret(base58): cWSoJQ9TQ8b2cuHAu9VEqHWT5SidzL1MX9xmif3B6PZQfmevRbCb
        # address(base58): nCWkg8kPkKDz3bWrFyySNPNYN1eJjVshJp

        # secret(base58): cZAPASDfrVk2xMk1Vm2g5nyxKFZNrozF6ia27dDu1JNXktL9jVSX
        # address(base58): nEXffHrEqPb21AQMK8T2uW2cWBFG6GjfHB

        # secret(base58): ceSjCTTatoznRFiPzHRd4NLhqe7Jb333fm9oCYQ2X7JYDvcmWknx
        # address(base58): n6p2GmroGpzzWKJZWW4JMA6kp7zWSVajki

        # secret(base58): cWaiBKc79taerRCoUPr8ccAWeE2njTvwA3w83ka2ZpQ1gw9V3uBf
        # address(base58): nJKqgmUGTpx8TL1TkPtC1RJZ9tGJxfU73x

        self.add_nodes(5)

        spork_chain_params =   ["-sporkaddr=nDQMa7iiKMXnZoU3CXaXs7VXvugYCjFsPn",
                                "-sporkaddr=nCWkg8kPkKDz3bWrFyySNPNYN1eJjVshJp",
                                "-sporkaddr=nEXffHrEqPb21AQMK8T2uW2cWBFG6GjfHB",
                                "-sporkaddr=n6p2GmroGpzzWKJZWW4JMA6kp7zWSVajki",
                                "-sporkaddr=nJKqgmUGTpx8TL1TkPtC1RJZ9tGJxfU73x",
                                "-minsporkkeys=3"]

        # Node0 extra args to use on normal node restarts
        self.node0_extra_args = ["-sporkkey=cXfsWQHV7koi4ScASoNMRFA3pGqC4Yp39BsSezRGA34FbF3biABP"] + spork_chain_params

        self.start_node(0, self.node0_extra_args)
        self.start_node(1, ["-sporkkey=cWSoJQ9TQ8b2cuHAu9VEqHWT5SidzL1MX9xmif3B6PZQfmevRbCb"] + spork_chain_params)
        self.start_node(2, ["-sporkkey=cZAPASDfrVk2xMk1Vm2g5nyxKFZNrozF6ia27dDu1JNXktL9jVSX"] + spork_chain_params)
        self.start_node(3, ["-sporkkey=ceSjCTTatoznRFiPzHRd4NLhqe7Jb333fm9oCYQ2X7JYDvcmWknx"] + spork_chain_params)
        self.start_node(4, ["-sporkkey=cWaiBKc79taerRCoUPr8ccAWeE2njTvwA3w83ka2ZpQ1gw9V3uBf"] + spork_chain_params)

        # connect nodes at start
        for i in range(0, 5):
            for j in range(i, 5):
                connect_nodes(self.nodes[i], j)

    def get_test_spork_value(self, node, spork_name):
        self.bump_mocktime(5)  # advance ProcessTick
        info = node.spork('show')
        # use InstantSend spork for tests
        return info[spork_name]

    def test_spork(self, spork_name, final_value):
        # check test spork default state
        for node in self.nodes:
            assert(self.get_test_spork_value(node, spork_name) == 4070908800)

        self.bump_mocktime(1)
        # first and second signers set spork value
        self.nodes[0].spork(spork_name, 1)
        self.nodes[1].spork(spork_name, 1)
        # spork change requires at least 3 signers
        time.sleep(10)
        for node in self.nodes:
            assert(self.get_test_spork_value(node, spork_name) != 1)

        # restart with no extra args to trigger CheckAndRemove
        self.restart_node(0)
        assert(self.get_test_spork_value(self.nodes[0], spork_name) != 1)

        # restart again with corect_params, should resync spork parts from other nodes
        self.restart_node(0, self.node0_extra_args)
        for i in range(1, 5):
            connect_nodes(self.nodes[0], i)

        # third signer set spork value
        self.nodes[2].spork(spork_name, 1)
        # now spork state is changed
        for node in self.nodes:
            wait_until(lambda: self.get_test_spork_value(node, spork_name) == 1, sleep=0.1, timeout=10)

        # restart with no extra args to trigger CheckAndRemove, should reset the spork back to its default
        self.restart_node(0)
        assert(self.get_test_spork_value(self.nodes[0], spork_name) == 4070908800)

        # restart again with corect_params, should resync sporks from other nodes
        self.restart_node(0, self.node0_extra_args)
        for i in range(1, 5):
            connect_nodes(self.nodes[0], i)

        wait_until(lambda: self.get_test_spork_value(self.nodes[0], spork_name) == 1, sleep=0.1, timeout=10)

        self.bump_mocktime(1)
        # now set the spork again with other signers to test
        # old and new spork messages interaction
        self.nodes[2].spork(spork_name, final_value)
        self.nodes[3].spork(spork_name, final_value)
        self.nodes[4].spork(spork_name, final_value)
        for node in self.nodes:
            wait_until(lambda: self.get_test_spork_value(node, spork_name) == final_value, sleep=0.1, timeout=10)

    def run_test(self):
        self.test_spork('SPORK_2_INSTANTSEND_ENABLED', 2)
        self.test_spork('SPORK_3_INSTANTSEND_BLOCK_FILTERING', 3)
        for node in self.nodes:
            assert(self.get_test_spork_value(node, 'SPORK_2_INSTANTSEND_ENABLED') == 2)
            assert(self.get_test_spork_value(node, 'SPORK_3_INSTANTSEND_BLOCK_FILTERING') == 3)


if __name__ == '__main__':
    MultiKeySporkTest().main()
