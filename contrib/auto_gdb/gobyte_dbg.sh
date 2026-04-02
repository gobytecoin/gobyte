#!/usr/bin/env bash
# use testnet settings,  if you need mainnet,  use ~/.gobytecore/gobyted.pid file instead
export LC_ALL=C

gobyte_pid=$(<~/.gobytecore/testnet3/gobyted.pid)
sudo gdb -batch -ex "source debug.gdb" gobyted "${gobyte_pid}"
