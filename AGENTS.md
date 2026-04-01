# Agent Guidelines for GoByte Core

This file provides development guidelines for AI agents working on the GoByte Core codebase.

## Project Overview

GoByte is a cryptocurrency node implementation and direct fork of Dash (which itself is a fork of Bitcoin Core), written primarily in C++ with Python functional tests. The codebase uses autotools for building. It includes masternode features, LLMQ (Long Living Masternode Quorums), InstantSend, ChainLocks, and uses the **NeoScrypt** algorithm for PoW mining (unlike Dash's X11).

## Directory Structure

```
src/
├── bench/              # Performance benchmarks
├── bls/                # BLS cryptographic operations
├── coinjoin/          # CoinJoin mixing (replaces PrivateSend)
├── compat/             # Compatibility layer
├── consensus/         # Consensus rules (merkle, tx_verify)
├── crypto/            # Cryptographic primitives (sha256, neoscrypt)
├── evo/               # Evolution/deterministic masternode data
├── governance/        # Governance system (proposals, voting)
├── immer/             # Immutable data structures
├── interfaces/        # Wallet/node interfaces
├── leveldb/           # LevelDB database
├── llmq/              # LLMQ (Long Living Masternode Quorums)
├── masternode/        # Masternode infrastructure
├── policy/            # Policy (fees, policy.cpp)
├── primitives/        # Transaction/block primitives
├── qt/                # GUI implementation (Qt 5)
├── rpc/               # JSON-RPC server and endpoints
├── script/            # Script interpreter
├── secp256k1/         # Elliptic curve cryptography
├── support/           # Utility functions
├── test/              # Unit tests (Boost::Test)
├── univalue/          # JSON library
├── wallet/            # Wallet implementation
└── zmq/               # ZeroMQ notifications
```

### Directories to Exclude

**Under any circumstances**, do not make changes to:
- `guix-build*` - Build system files
- `releases` - Release artifacts
- Vendored dependencies:
  - `src/{leveldb,secp256k1,univalue,bls,immer}`
  - `src/crypto/ctaes` - AES encryption

**Unless specifically prompted**, avoid:
- `.github` - GitHub workflows and configs
- `depends` - Dependency build system
- `ci` - Continuous integration
- `contrib` - Contributed scripts
- `doc` - Documentation

## Build Commands

### Initial Setup
```bash
./autogen.sh
./configure [options]
make
```

### Common Configure Options
```bash
./configure --enable-debug          # Debug build with symbols
./configure --disable-tests          # Skip building tests
./configure --disable-bench          # Skip building benchmarks
./configure --enable-lcov            # Enable code coverage
./configure --with-sanitizers=address,undefined  # Address/undefined sanitizers
```

### Running Tests
```bash
make check                           # Run all unit tests
src/test/test_gobyte                 # Run tests directly
test_gobyte --run_test=suite_name    # Run specific test suite
test_gobyte --run_test=suite_name/test_case  # Run specific test case
test_gobyte --log_level=all --run_test=suite_name  # Verbose output
test_gobyte --help                  # Show available options
```

### Running Functional Tests
```bash
test/functional/test_runner.py          # Run all functional tests
test/functional/test_runner.py --extended   # Run extended tests
test/functional/test_runner.py feature_rbf.py  # Run specific test
test/functional/test_runner.py --coverage    # Track RPC coverage
test/functional/test_runner.py -j$(nproc)  # Parallel execution
```

### Linting
```bash
test/lint/lint-all.sh             # Run all lint checks
test/lint/lint-whitespace.sh     # Whitespace checks
contrib/devtools/clang-format-diff.py   # Check formatting
test/lint/check-rpc-mappings.py  # RPC consistency
```

## C++ Code Style

### Formatting (clang-format)
- Use `src/.clang-format` for automatic formatting
- 4 space indentation (no tabs) for every block except namespaces
- No indentation for `public`/`protected`/`private` or `namespace`
- Braces on new lines for classes/functions/methods
- Braces on same line for control structures
- No extra spaces inside parentheses
- No space after function names; one space after `if`, `for`, `while`
- Single-line `if` without braces allowed; otherwise use braces

### Naming Conventions
- **Variables/functions**: snake_case (e.g., `my_variable`, `get_value`)
- **Class names**: UpperCamelCase (e.g., `MyClass`) - avoid C prefix
- **Member variables**: `m_` prefix (e.g., `m_count`)
- **Global variables**: `g_` prefix (e.g., `g_count`)
- **Constants**: ALL_CAPS with underscores
- **Test suites**: `<source>_tests` (e.g., `getarg_tests`)

### Imports and Organization
- Use bracket syntax `#include <foo/bar.h>` not quotes
- Include every header file directly used
- Use include guards: `#ifndef BITCOIN_FOO_BAR_H`
- Terminate namespaces with comment: `} // namespace foo`
- Don't use `using namespace ...` in headers

### Types and Casting
- Use `nullptr` not `NULL` or `(void*)0`
- Use list initialization: `int{x}`
- Prefer `static_cast`, `reinterpret_cast`, `const_cast` over C-style casts
- Use `uint8_t` instead of bare `char`
- Align pointers/references left: `type& var` not `type &var`
- Prefer `std::optional` for optional return values
- Use `Span<T>` for flexible container parameters

### Error Handling
- Use `Assert()` or `assert` for unrecoverable logic bugs
- Use `CHECK_NONFATAL` for recoverable internal errors
- Use `Assume()` for assumptions that don't require termination
- Never use assertions/checks for user/network input validation
- Enable `-DDEBUG_LOCKORDER` when testing locking code
- `static_assert` preferred over `assert` where possible

### Logging
- `LogPrintf(...)`: Direct printf-style logging (use instead of printf/printf)
- `LogPrint(category, ...)`: Category-based debug logging (e.g., `LogPrint(BCLog::MEMPOOL, "msg")`)

### Doxygen Comments
```cpp
/** Description of function
 * @param[in] arg1 Description
 * @param[in] arg2 Another description
 * @pre Precondition for function
 */
bool function(int arg1, const char* arg2);

// For members:
int var; //!< Description after member
//! Description before member
int var2;
```

## Important Global Variables
```cpp
extern CCriticalSection cs_main;    // validation.h - Main chain state
extern CTxMemPool mempool;          // validation.h - Transaction mempool
extern std::unique_ptr<CConnman> g_connman;  // net.h - Network manager
extern std::atomic_bool g_is_mempool_loaded; // validation.h - Mempool loaded state
extern BlockMap& mapBlockIndex;     // validation.h - Block index map
```

## Locking Model
```cpp
{
    LOCK(cs_main);       // ALWAYS use braces with locks
}
LOCK2(cs_main, cs_vNodes);  // Multiple locks
AssertLockHeld(cs_main);   // Check lock held
AssertLockNotHeld(cs_main);
```

## High-Level Architecture

GoByte extends Dash through composition:

```
GoByte Components
├── NeoScrypt (PoW - replaces Dash's X11)
├── Dash Foundation
│   ├── Masternodes (Infrastructure)
│   │   ├── LLMQ (Quorum infrastructure)
│   │   │   ├── InstantSend (Transaction locking)
│   │   │   ├── ChainLocks (Block finality)
│   │   │   └── Platform (GoByte Platform support via LLMQ_100_67)
│   │   ├── CoinJoin (Coin mixing)
│   │   └── Governance Voting
│   └── Spork System (Feature control)
└── Bitcoin Core Foundation (Blockchain, consensus, networking)
```

### Key Components

- **Masternodes** (`src/masternode/`, `src/evo/`): Deterministic masternode lists, special transactions (ProRegTx, ProUpServTx, etc.)
- **LLMQ** (`src/llmq/`): Long Living Masternode Quorums for ChainLocks, InstantSend, governance, and Platform
- **CoinJoin** (`src/coinjoin/`): Coin mixing for privacy
- **Governance** (`src/governance/`): Proposals, voting, treasury
- **NeoScrypt** (`src/crypto/neoscrypt.cpp`): GoByte's PoW mining algorithm

### GoByte-Specific Databases

- **CFlatDB**: Flat file database for GoByte-specific data
  - `CMasternodeMetaMan`: Masternode metadata persistence (`mncache.dat`)
  - `CGovernanceManager`: Governance object storage (`governance.dat`)
  - `CSporkManager`: Spork state persistence (`sporks.dat`)
  - `CNetFulfilledRequestManager`: Network request tracking (`netfulfilled.dat`)
- **CDBWrapper**: LevelDB wrapper for blockchain data
  - `CEvoDB`: Specialized database for deterministic masternode data
  - `CDKGSessionManager`: LLMQ DKG session persistence
  - `CQuorumManager`: Quorum state storage
  - `CRecoveredSigsDb`: LLMQ recovered signature storage
  - `CInstantSendDb`: InstantSend lock persistence

### Integration Patterns

#### Initialization Flow
1. **Basic Setup**: Core Bitcoin/Dash initialization
2. **Parameter Interaction**: GoByte-specific configuration validation
3. **Interface Setup**: GoByte manager instantiation in NodeContext
4. **Main Initialization**: EvoDb, masternode system, LLMQ, governance startup

#### Consensus Integration
- **Block Validation Extensions**: Special transaction validation
- **Mempool Extensions**: Enhanced transaction relay
- **Chain State Extensions**: Masternode list and quorum state tracking
- **Fork Prevention**: ChainLocks prevent reorganizations

#### Key Design Patterns
- **Manager Pattern**: Centralized managers for each subsystem
- **Event-Driven Architecture**: ValidationInterface callbacks coordinate subsystems
- **Immutable Data Structures**: Efficient masternode list management using Immer library
- **Extension Over Modification**: Minimal changes to Bitcoin Core/Dash foundation

### Critical Interfaces
- **NodeContext**: Central dependency injection container
- **ValidationInterface**: Event distribution for block/transaction processing
- **ChainstateManager**: Enhanced with GoByte-specific validation
- **Special Transaction Serialization**: Payload serialization routines (`src/evo/specialtx.h`)
- **BLS Integration**: Cryptographic foundation for advanced features

## Python Code Style (Functional Tests)
- Follow PEP-8, use flake8
- Avoid wildcard imports
- Use `'{}'.format(x)` not `'%s' % x`
- Test file naming: `<area>_test.py` (feature, rpc, wallet, p2p, etc.)
- Use `BitcoinTestFramework` base class

## RPC Guidelines
- Method names: lowercase with underscores (`getrawtransaction`)
- Use JSON parser for arguments
- Add arguments to `vRPCConvertParams` in `rpc/client.cpp` for name-based calling
- RPC methods must be wallet OR non-wallet, not both

## Debugging Tips
```bash
# Full debug logging
./src/gobyted -debug -debug=1 -log-level=trace

# Category-specific
./src/gobyted -debug=mempool -debug=net

# Memory debugging
valgrind --suppressions=contrib/valgrind.supp src/test/test_gobyte

# Debug with gdb
gdb ./src/gobyted
```

## Development Workflow

### Common Tasks
```bash
# Clean build
make clean

# Run gobyted with debug logging
./src/gobyted -debug=all -printtoconsole
```

## Adding New Code

### Adding a New RPC
1. Declare in `rpc/server.h` using `RPCHelpMan`
2. Implement in appropriate `rpc/*.cpp`
3. Register in `rpc/server.cpp` using `.AppendHelp()` and `.AppendCommand()`
4. Add to `vRPCConvertParams` in `rpc/client.cpp` for named args

### Adding a New Unit Test
1. Add test file in `src/test/` named `<feature>_tests.cpp`
2. Add to `src/Makefile.test.include`
3. Use BOOST_AUTO_TEST_SUITE and BOOST_AUTO_TEST_CASE

## Common Pitfalls
1. **Lock ordering**: Always acquire `cs_main` before other locks
2. **map[key]**: Use `.find()` - `map[key]` for reading inserts!
3. **Bare char**: Use `uint8_t` or `int8_t`
4. **Include guards**: Always use `#ifndef` in headers
5. **Signed/unsigned**: Be careful with integer comparisons
6. **Null pointer**: Check before logging objects
7. **C-style casts**: Use `static_cast<>` instead
8. **Member init**: Use member initialization: `m_count{0}`

## Key Files
- `src/Makefile.am`: Main build configuration
- `src/Makefile.test.include`: Unit test sources
- `src/.clang-format`: C++ formatting rules
- `src/validation.cpp`: Main chain validation (cs_main)
- `src/net_processing.cpp`: P2P message handling
- `src/rpc/server.cpp`: RPC registration
- `test/functional/`: Functional test suite

## Branch Structure

- `master`: Stable releases
- `develop`: Active development (built and tested regularly)

## Git/PR Guidelines
- Use Conventional Commits: `feat(rpc): description`, `fix(log): description`
- Commit messages: 50 char subject, blank line, detailed body
- Reference issues with `refs #1234` or `fixes #4321`
- Don't submit style-only changes
- Squash fixup commits before review
