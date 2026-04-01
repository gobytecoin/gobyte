# GoByte Core version 0.16.2.2
===============================

This is a new minor version release, bringing various bugfixes and performance improvements.
This release is **optional** for all nodes, although recommended.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/gobytecoin/gobyte/issues>


# Upgrading and downgrading

## How to Upgrade

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes in some cases), then run the
installer (on Windows) or just copy over `/Applications/GoByte-Qt` (on macOS) or
`gobyted`/`gobyte-cli`/`gobyte-qt` (on Linux).

## Downgrade warning

### Downgrade to a version < **0.16.2.X**

Downgrading to a version older than **0.16.2.X** may not be supported, and will
likely require a reindex.

# Compatibility

GoByte Core is supported and tested on operating systems using the
Linux Kernel 3.17+, macOS 14+, and Windows 10+. GoByte Core
should also work on most other Unix-like systems but is not as
frequently tested on them. It is not recommended to use GoByte Core on
unsupported systems.

# Release Notes

Notable changes
===============

- Fixed a critical bug that caused testnet nodes to permanently stall at block 87682. The issue was that null quorum commitments (harmless zero-value commitments) referencing unregistered LLMQ types (like type 102 from v0.17 dev builds) were incorrectly rejected. The fix reorders validation checks so null commitments are validated before checking if the LLMQ type is registered - since null commitments commit no quorum state, they are safe to accept regardless of type registration. (gobyte#49)

- Removed the unused `LLMQ_5_60` quorum type (which conflicted with `LLMQ_TEST`) and updated testnet to use `LLMQ_50_60` (50 members, 30 threshold) instead. This aligns testnet with mainnet and upstream Dash configuration, and prepares for a testnet restart with standardized LLMQ parameters. (gobyte#50)

- Complete testnet reset due to incompatible blocks containing LLMQ types (102, 130) that v0.16.x nodes cannot validate. Changes include: new genesis block (hash: `0x00000488...`), all deployments set to activate April 1st, 2026 with infinite timeout, cleared old checkpoints, updated spork keys. All existing testnet data becomes invalid - nodes must resync from genesis. (gobyte#52)

- Re-enabled BIP34 coinbase height validation (previously commented out) and corrected `BIP34Height` from 1 to 17. This strengthens block validation by ensuring coinbase transactions contain the correct block height encoding, aligning the codebase with upstream Dash and reducing technical debt. Only affects pre-DIP0003 blocks. (gobyte#47)

- Removed a hardcoded version check in `net_processing.cpp` that automatically disconnected any peer not running version 0.16.x. This check was overly restrictive and prevented network interoperability with nodes running different (compatible) versions. (gobyte#44)


Low-level changes
=================

- [263a25a86] chore: update copyright year in configure.ac
- [7b1833001] chore(depends): update dependency download mirrors to reliable sources
- [4bd9935b8] chore: update GPG key
- [534be8a33] chore: update testnet seeds
- [b6e1a708b] chore: Add issues templates
- [9e69306ef] chore: Add SECURITY.md
- [bde5c7d3d] chore: add release nodes template
- [4e0dc56cf] chore: update docs inside /doc/


See detailed [set of changes][set-of-changes].

# Credits

Thanks to everyone who directly contributed to this release:

- D0WN3D
- sirbond
- The Bitcoin Core Developers
- The Dash Core Developers

As well as everyone that submitted issues, reviewed pull requests and helped
debug the release candidates.

# Older releases

These releases are considered obsolete. Old release notes can be found here:

- [v16.2.1](https://github.com/gobytecoin/gobyte/blob/master/doc/release-notes/gobyte/release-notes-0-16.2.1.md) released Dec/06/2021
- [v16.1.1](https://github.com/gobytecoin/gobyte/blob/master/doc/release-notes/gobyte/release-notes-0-16.1.1.md) released Oct/05/2021
- [v12.2.4](https://github.com/gobytecoin/gobyte/blob/master/doc/release-notes/gobyte/release-notes-0-12.2.4.md) released Jun/18/2018

[set-of-changes]: https://github.com/gobytecoin/gobyte/compare/0.16.2.1...gobytecoin:0.16.2.2
