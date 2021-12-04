#!/usr/bin/env python3
# Copyright (c) 2018-2020 The Dash Core developers
# Copyright (c) 2017-2021 The GoByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import connect_nodes, wait_until

'''
multikeysporks.py

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
        # secret(base58): 942MmdVMK95TdFdCYK3mkkC6MPx9GxNuMjy7qGmwN9cfDZ6JvfF
        # keyid(hex): 60f0f57f71f0081f1aacdd8432340a33a526f91b
        # address(base58): nNFe5kSc6KcuezjZavqizjfuqY9rj5JTxh

        # secret(base58): 94YkGAQ4igCsyH98hwg3UMfH5f4uSZCDmU5FGLgGXwu8umRG1Hx
        # keyid(hex): 43dff2b09de2f904f688ec14ee6899087b889ad0
        # address(base58): n8hdGQt6N9BgCnTmqwrEHzsTGLm7MwbNst

        # secret(base58): 93fRQ7X4g4XFQDAXhdT9o7rJKeXcuoeCe6rkCcRQGDzhMEK4j5d
        # keyid(hex): d9aa5fa00cce99101a4044e65dc544d1579890de
        # address(base58): nPCNoPqJ57ENu6uZfnimi267vLgsuT6HFv

        # secret(base58): 957eM6g5YUhPCxPm5SLPgFXuWYAQ3ixELcNzFfpqGzjCGrUUP67
        # keyid(hex): 0b23935ce0bea3b997a334f6fa276c9fa17687b2
        # address(base58): n7LfDRBkE65bhpwqT4MzJRB6QxKsATWUdA

        # secret(base58): 94N31JxLB5dV1cPruNtZAuonE4ttxyBQ36bPc3omD3MCYiWjPNt
        # keyid(hex): 1d1098b2b1f759b678a0a7a098637a9b898adcac
        # address(base58): nJSbynYfQVWBnC5SdDTTzDD8bpvJ4ftD6t

        self.add_nodes(5)

        self.start_node(0, ["-sporkkey=942MmdVMK95TdFdCYK3mkkC6MPx9GxNuMjy7qGmwN9cfDZ6JvfF",
                            "-sporkaddr=nPCNoPqJ57ENu6uZfnimi267vLgsuT6HFv",
                            "-sporkaddr=n8hdGQt6N9BgCnTmqwrEHzsTGLm7MwbNst",
                            "-sporkaddr=nNFe5kSc6KcuezjZavqizjfuqY9rj5JTxh",
                            "-sporkaddr=n7LfDRBkE65bhpwqT4MzJRB6QxKsATWUdA",
                            "-sporkaddr=nJSbynYfQVWBnC5SdDTTzDD8bpvJ4ftD6t",
                            "-minsporkkeys=3"])
        self.start_node(1, ["-sporkkey=94YkGAQ4igCsyH98hwg3UMfH5f4uSZCDmU5FGLgGXwu8umRG1Hx",
                            "-sporkaddr=nPCNoPqJ57ENu6uZfnimi267vLgsuT6HFv",
                            "-sporkaddr=n8hdGQt6N9BgCnTmqwrEHzsTGLm7MwbNst",
                            "-sporkaddr=nNFe5kSc6KcuezjZavqizjfuqY9rj5JTxh",
                            "-sporkaddr=n7LfDRBkE65bhpwqT4MzJRB6QxKsATWUdA",
                            "-sporkaddr=nJSbynYfQVWBnC5SdDTTzDD8bpvJ4ftD6t",
                            "-minsporkkeys=3"])
        self.start_node(2, ["-sporkkey=93fRQ7X4g4XFQDAXhdT9o7rJKeXcuoeCe6rkCcRQGDzhMEK4j5d",
                            "-sporkaddr=nPCNoPqJ57ENu6uZfnimi267vLgsuT6HFv",
                            "-sporkaddr=n8hdGQt6N9BgCnTmqwrEHzsTGLm7MwbNst",
                            "-sporkaddr=nNFe5kSc6KcuezjZavqizjfuqY9rj5JTxh",
                            "-sporkaddr=n7LfDRBkE65bhpwqT4MzJRB6QxKsATWUdA",
                            "-sporkaddr=nJSbynYfQVWBnC5SdDTTzDD8bpvJ4ftD6t",
                            "-minsporkkeys=3"])
        self.start_node(3, ["-sporkkey=957eM6g5YUhPCxPm5SLPgFXuWYAQ3ixELcNzFfpqGzjCGrUUP67",
                            "-sporkaddr=nPCNoPqJ57ENu6uZfnimi267vLgsuT6HFv",
                            "-sporkaddr=n8hdGQt6N9BgCnTmqwrEHzsTGLm7MwbNst",
                            "-sporkaddr=nNFe5kSc6KcuezjZavqizjfuqY9rj5JTxh",
                            "-sporkaddr=n7LfDRBkE65bhpwqT4MzJRB6QxKsATWUdA",
                            "-sporkaddr=nJSbynYfQVWBnC5SdDTTzDD8bpvJ4ftD6t",
                            "-minsporkkeys=3"])
        self.start_node(4, ["-sporkkey=94N31JxLB5dV1cPruNtZAuonE4ttxyBQ36bPc3omD3MCYiWjPNt",
                            "-sporkaddr=nPCNoPqJ57ENu6uZfnimi267vLgsuT6HFv",
                            "-sporkaddr=n8hdGQt6N9BgCnTmqwrEHzsTGLm7MwbNst",
                            "-sporkaddr=nNFe5kSc6KcuezjZavqizjfuqY9rj5JTxh",
                            "-sporkaddr=n7LfDRBkE65bhpwqT4MzJRB6QxKsATWUdA",
                            "-sporkaddr=nJSbynYfQVWBnC5SdDTTzDD8bpvJ4ftD6t",
                            "-minsporkkeys=3"])
        # connect nodes at start
        for i in range(0, 5):
            for j in range(i, 5):
                connect_nodes(self.nodes[i], j)

    def get_test_spork_value(self, node):
        info = node.spork('show')
        # use InstantSend spork for tests
        return info['SPORK_2_INSTANTSEND_ENABLED']

    def set_test_spork_value(self, node, value):
        # use InstantSend spork for tests
        node.spork('SPORK_2_INSTANTSEND_ENABLED', value)

    def run_test(self):
        # check test spork default state
        for node in self.nodes:
            assert(self.get_test_spork_value(node) == 4070908800)

        self.bump_mocktime(1)
        # first and second signers set spork value
        self.set_test_spork_value(self.nodes[0], 1)
        self.set_test_spork_value(self.nodes[1], 1)
        # spork change requires at least 3 signers
        time.sleep(10)
        for node in self.nodes:
            assert(self.get_test_spork_value(node) != 1)

        # third signer set spork value
        self.set_test_spork_value(self.nodes[2], 1)
        # now spork state is changed
        for node in self.nodes:
            wait_until(lambda: self.get_test_spork_value(node) == 1, sleep=0.1, timeout=10)

        self.bump_mocktime(1)
        # now set the spork again with other signers to test
        # old and new spork messages interaction
        self.set_test_spork_value(self.nodes[2], 2)
        self.set_test_spork_value(self.nodes[3], 2)
        self.set_test_spork_value(self.nodes[4], 2)
        for node in self.nodes:
            wait_until(lambda: self.get_test_spork_value(node) == 2, sleep=0.1, timeout=10)


if __name__ == '__main__':
    MultiKeySporkTest().main()
