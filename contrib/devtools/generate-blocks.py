#!/usr/bin/env python3
"""
GoByte block generator using NeoScrypt.

Mines blocks locally (CPU) and submits them via the `submitblock` RPC.
Works from genesis during IBD — does NOT require the node to be synced.
Supports both testnet (n-prefix) and mainnet (G-prefix) addresses.

Usage:
    python3 generate-blocks.py [--powlimit <hex>]

    --powlimit  Override the nBits value used for all mined blocks.
                Use this when the node rejects with "bad-diffbits".
                The correct value is CTestNetParams::consensus.powLimit
                in compact (nBits) form, e.g. --powlimit 0x207fffff.
                To find it:
                  grep -A2 "powLimit" src/chainparams.cpp
                Then convert: python3 -c "
                    p = 0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
                    e = (p.bit_length() + 7) // 8
                    m = p >> (8*(e-3)) & 0x7fffff
                    print(hex((e << 24) | m))"

Requirements:
    - GoByte node running with RPC enabled (-server)
    - RPC credentials configured in gobyte.conf
    - Script run from inside the GoByte repository (for neoscrypt.cpp)

from __future__ import annotations

import argparse
import base64
import ctypes
import hashlib
import http.client
import json
import os
import platform
import struct
import subprocess
import sys
import tempfile
import time


# ── NeoScrypt loader ─────────────────────────────────────────────────────────

# Smoke-test vector: regtest genesis header → known-good hash.
# Used to verify the compiled shared library produces correct output
# before trusting it with real block mining.
_SMOKE_HEADER = (
    struct.pack("<I", 1)
    + bytes(32)
    + bytes.fromhex(
        "dc9a719dc1bcda39107ea55424f00cab512170a1cb69efa08531f483f2399f21"
    )[::-1]
    + struct.pack("<I", 1510727100)
    + struct.pack("<I", 0x1E0FFFF0)
    + struct.pack("<I", 901219)
)
_SMOKE_EXPECTED = "00000dbc9aa1686b4dfb177300185c6a3e0b13d1d4d346c5bccdd19fdf9ebc5a"


def _verify_fn(fn) -> bool:
    try:
        result = fn(_SMOKE_HEADER)[::-1].hex()
        return result == _SMOKE_EXPECTED
    except Exception:
        return False


def _load_so(path: str):
    try:
        lib = ctypes.CDLL(path)
        lib.neoscrypt.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32]
        lib.neoscrypt.restype = None
        _out = ctypes.create_string_buffer(32)

        def fn(header: bytes) -> bytes:
            lib.neoscrypt(header, _out, 0x0)
            return bytes(_out)

        return fn if _verify_fn(fn) else None
    except Exception:
        return None


def _find_repo_source() -> str | None:
    check = os.path.dirname(os.path.abspath(__file__))
    for _ in range(8):
        for name in ("neoscrypt.c", "neoscrypt.cpp"):
            candidate = os.path.join(check, "src", "crypto", name)
            if os.path.isfile(candidate):
                return candidate
        parent = os.path.dirname(check)
        if parent == check:
            break
        check = parent
    return None


def _compile_from_source(src_file: str):
    src_dir = os.path.dirname(src_file)
    system = platform.system()
    lib_name = (
        "neoscrypt_genesis.dylib" if system == "Darwin" else "neoscrypt_genesis.so"
    )
    lib_path = os.path.join(tempfile.gettempdir(), lib_name)
    shared_flag = "-dynamiclib" if system == "Darwin" else "-shared"

    compiler_tries = [
        (["clang", "gcc", "cc"], ["-x", "c"]),
        (["clang++", "g++", "c++"], []),
    ]

    for compilers, extra_flags in compiler_tries:
        for compiler in compilers:
            try:
                cmd = (
                    [compiler]
                    + extra_flags
                    + [shared_flag, "-fPIC", "-O2", f"-I{src_dir}",
                       "-o", lib_path, src_file]
                )
                r = subprocess.run(cmd, capture_output=True, timeout=60)
                if r.returncode != 0:
                    continue
                fn = _load_so(lib_path)
                if fn is not None:
                    return fn
            except (FileNotFoundError, subprocess.TimeoutExpired):
                continue
    return None


def _load_neoscrypt():
    src = _find_repo_source()
    if src is not None:
        print(f"[neoscrypt] Compiling from {os.path.relpath(src)} …", flush=True)
        fn = _compile_from_source(src)
        if fn is not None:
            print("[neoscrypt] Ready.\n", flush=True)
            return fn
        print("[neoscrypt] Compilation failed.\n", flush=True)

    sys.exit(
        "\nERROR: Could not load NeoScrypt.\n\n"
        "Run this script from inside the GoByte repository so it\n"
        "can find and compile src/crypto/neoscrypt.cpp automatically.\n"
    )


# ── Target calculation ────────────────────────────────────────────────────────

def _nbits_to_target(nbits: int) -> int:
    exp = (nbits >> 24) & 0xFF
    mantissa = nbits & 0x7FFFFF
    return mantissa * (1 << (8 * (exp - 3)))


# ── Testnet Genesis Block ─────────────────────────────────────────────────────

TESTNET_GENESIS = {
    "nVersion": 1,
    "hashPrevBlock": "0000000000000000000000000000000000000000000000000000000000000000",
    "hashMerkleRoot": "dc9a719dc1bcda39107ea55424f00cab512170a1cb69efa08531f483f2399f21",
    "nTime": 1774483200,
    "nBits": 0x1E0FFFF0,
    "nNonce": 359663,
    "hash": "00000b103faa9ba192192fa8905fc5b8a11bf79026fa089f8c0742cb79559b56",
}

# Testnet powLimit in compact (nBits) format.
# From CTestNetParams: consensus.powLimit = ~uint256(0) >> 20
# This converts to compact form 0x1e0fffff (NOT 0x1e0ffff0 which is the genesis nBits).
# The node uses this value for early blocks (height < 24) and when the
# testnet min-difficulty rule applies (block time > prev_time + 300s).
TESTNET_POWLIMIT_NBITS = 0x1E0FFFFF

# Testnet difficulty adjustment parameters
TESTNET_POW_TARGET_SPACING = 150  # 2.5 minutes in seconds
TESTNET_MIN_DIFFICULTY_WINDOW = TESTNET_POW_TARGET_SPACING * 2  # 300 seconds

# Known GoByte address version bytes (base58check leading byte).
#   Mainnet P2PKH = 38  → 'G' prefix
#   Testnet P2PKH = 112 → 'n' prefix  (same as Bitcoin/Dash testnet)
# Both are accepted silently.  Any other version byte triggers a warning
# but does not abort — the hash160 extraction is version-byte-agnostic.
_KNOWN_VERSION_BYTES: dict[int, str] = {
    38: "mainnet (G-prefix)",
    112: "testnet (n-prefix)",
}


# ── RPC Client ────────────────────────────────────────────────────────────────

def rpc_request(host: str, port: int, user: str, password: str,
                method: str, params=None):
    if params is None:
        params = []

    payload = json.dumps({
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": 1,
    })

    auth = base64.b64encode(f"{user}:{password}".encode()).decode()
    headers = {
        "Authorization": f"Basic {auth}",
        "Content-Type": "application/json",
    }

    conn = http.client.HTTPConnection(host, port, timeout=30)
    try:
        conn.request("POST", "/", body=payload, headers=headers)
        resp = conn.getresponse()
        body = resp.read()
    except http.client.HTTPException as exc:
        raise RuntimeError(
            f"HTTP error communicating with {host}:{port}: {exc}"
        ) from exc
    except OSError as exc:
        # Covers ConnectionRefusedError, TimeoutError, etc.
        raise RuntimeError(
            f"Could not connect to {host}:{port} — is the GoByte node running? ({exc})"
        ) from exc
    finally:
        conn.close()

    try:
        resp_json = json.loads(body)
    except json.JSONDecodeError as exc:
        raise RuntimeError(
            f"Non-JSON response from node (HTTP {resp.status}): {body[:200]!r}"
        ) from exc

    if resp_json.get("error"):
        raise RuntimeError(f"RPC error: {resp_json['error']}")

    return resp_json.get("result")


# ── Block building ────────────────────────────────────────────────────────────

def _encode_varint(n: int) -> bytes:
    if n < 0xFD:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xFD" + struct.pack("<H", n)
    if n <= 0xFFFFFFFF:
        return b"\xFE" + struct.pack("<I", n)
    return b"\xFF" + struct.pack("<Q", n)


def _encode_script_num(n: int) -> bytes:
    """CScriptNum sign-magnitude little-endian encoding."""
    if n == 0:
        return b""
    result = bytearray()
    neg = n < 0
    absval = abs(n)
    while absval:
        result.append(absval & 0xFF)
        absval >>= 8
    if result[-1] & 0x80:
        result.append(0x80 if neg else 0x00)
    elif neg:
        result[-1] |= 0x80
    return bytes(result)


def _push_data(data: bytes) -> bytes:
    """Minimal-push scriptSig encoding (Bitcoin Script rules)."""
    n = len(data)
    if n < 0x4C:
        return bytes([n]) + data
    if n <= 0xFF:
        return b"\x4C" + bytes([n]) + data
    if n <= 0xFFFF:
        return b"\x4D" + struct.pack("<H", n) + data
    return b"\x4E" + struct.pack("<I", n) + data


_BASE58_ALPHABET = b"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"


def _address_to_p2pkh_script(address: str) -> bytes:
    """
    Decode a base58check GoByte address and return the corresponding
    P2PKH scriptPubKey:
        OP_DUP OP_HASH160 <push 20> <hash160> OP_EQUALVERIFY OP_CHECKSIG

    Raises ValueError on checksum failure or an address that does not
    decode to the expected 25-byte payload (1 version + 20 hash160 + 4 checksum).
    """
    # Base58 decode
    num = 0
    for char in address.encode("ascii"):
        digit = _BASE58_ALPHABET.find(char)
        if digit < 0:
            raise ValueError(f"Invalid base58 character '{chr(char)}' in address")
        num = num * 58 + digit

    # 25 bytes: [version 1B][hash160 20B][checksum 4B]
    try:
        decoded = num.to_bytes(25, "big")
    except OverflowError as exc:
        raise ValueError(f"Address decodes to unexpected length: {address!r}") from exc

    version = decoded[0]
    payload = decoded[:21]
    checksum_stored = decoded[21:]
    checksum_computed = hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]

    if checksum_stored != checksum_computed:
        raise ValueError(
            f"Address checksum mismatch for {address!r} — "
            f"stored={checksum_stored.hex()}, computed={checksum_computed.hex()}"
        )

    if version not in _KNOWN_VERSION_BYTES:
        print(
            f"  [Warning] Address version byte {version} (0x{version:02x}) is not a "
            f"known GoByte version.  Expected mainnet=38.  Proceeding — verify "
            f"CTestNetParams::base58Prefixes[PUBKEY_ADDRESS] in chainparams.cpp."
        )

    hash160 = payload[1:21]  # 20-byte HASH160 of the public key

    # OP_DUP(0x76) OP_HASH160(0xa9) OP_PUSH20(0x14) <hash160> OP_EQUALVERIFY(0x88) OP_CHECKSIG(0xac)
    return bytes([0x76, 0xa9, 0x14]) + hash160 + bytes([0x88, 0xac])


def build_coinbase_tx(height: int, address: str,
                      reward_satoshis: int | None = None) -> bytes:
    """
    Build a coinbase transaction paying `reward_satoshis` to `address`.

    The output script is a standard P2PKH scriptPubKey.  The scriptSig
    encodes the block height as required by BIP34.

    If reward_satoshis is None (default), the correct block subsidy is
    automatically calculated based on the block height:
      - Height 1: 850,000 GBX (genesis subsidy, special case)
      - Height 2+: 15 GBX (regular subsidy, decreases ~8.333% yearly
        after each halving interval of 210,240 blocks).
    """
    if reward_satoshis is None:
        if height == 1:
            # Genesis block (height 1) has a special subsidy of 850,000 GBX.
            # GetBlockSubsidy() returns this when nPrevHeight == 0.
            reward_satoshis = 850_000 * 100_000_000  # 850,000 GBX in satoshis
        else:
            # Regular blocks start at 15 GBX.
            reward_satoshis = 1_500_000_000  # 15 GBX in satoshis
    output_script = _address_to_p2pkh_script(address)

    script_sig = (
        _push_data(_encode_script_num(height))
        + _push_data(b"GoByte testnet miner")
    )

    vin = (
        bytes(32)                                       # prev txid (null for coinbase)
        + struct.pack("<I", 0xFFFFFFFF)                 # prev vout (0xFFFFFFFF for coinbase)
        + _encode_varint(len(script_sig)) + script_sig
        + struct.pack("<I", 0xFFFFFFFF)                 # sequence
    )

    vout = (
        struct.pack("<q", reward_satoshis)
        + _encode_varint(len(output_script)) + output_script
    )

    return (
        struct.pack("<i", 1)        # nVersion
        + _encode_varint(1) + vin   # 1 input
        + _encode_varint(1) + vout  # 1 output
        + struct.pack("<I", 0)      # nLockTime
    )


def build_block_header(prev_block: str, merkle_root: str,
                       ntime: int, nbits: int, nonce: int) -> bytes:
    """
    Serialize an 80-byte block header in little-endian wire format.
    prev_block and merkle_root are display-order (big-endian) hex strings;
    they are byte-reversed here for on-wire serialization.
    """
    return (
        struct.pack("<I", 1)
        + bytes.fromhex(prev_block)[::-1]
        + bytes.fromhex(merkle_root)[::-1]
        + struct.pack("<I", ntime)
        + struct.pack("<I", nbits)
        + struct.pack("<I", nonce)
    )


def serialize_block(block_data: dict, coinbase_hex: str, nonce: int) -> str:
    """Serialize a complete block (header + 1 coinbase tx) for submitblock."""
    block = bytearray()
    block.extend(struct.pack("<I", block_data["nVersion"]))
    block.extend(bytes.fromhex(block_data["hashPrevBlock"])[::-1])
    block.extend(bytes.fromhex(block_data["hashMerkleRoot"])[::-1])
    block.extend(struct.pack("<I", block_data["nTime"]))
    block.extend(struct.pack("<I", block_data["nBits"]))
    block.extend(struct.pack("<I", nonce))
    block.extend(_encode_varint(1))             # tx count: 1 (coinbase only)
    block.extend(bytes.fromhex(coinbase_hex))
    return block.hex()


# ── Mining ────────────────────────────────────────────────────────────────────

def mine_block(block_data: dict, neoscrypt_fn) -> tuple[int, str]:
    """
    Iterate nonces until a NeoScrypt hash meets the block target.

    Returns (nonce, block_hash_hex) where block_hash_hex is in display
    (big-endian) order, matching the format expected by the next block's
    hashPrevBlock field.
    """
    prev_block = block_data["hashPrevBlock"]
    merkle_root = block_data["hashMerkleRoot"]
    ntime = block_data["nTime"]
    nbits = block_data["nBits"]
    target = _nbits_to_target(nbits)

    print(f"  nBits:          0x{nbits:08x}")
    print(f"  Target:         {target:#066x}")
    print(f"  Previous block: {prev_block[:16]}…")

    t0 = time.time()

    for nonce in range(0, 0xFFFF_FFFF):
        header = build_block_header(prev_block, merkle_root, ntime, nbits, nonce)
        raw = neoscrypt_fn(header)
        hash_int = int.from_bytes(raw, "little")

        if hash_int <= target:
            elapsed = time.time() - t0
            rate = nonce / elapsed if elapsed > 0 else float("inf")
            print(f"  Found nonce {nonce} in {elapsed:.2f}s ({rate:.0f} H/s)")
            # raw is little-endian; reverse to display (big-endian) order
            return nonce, raw[::-1].hex()

        if nonce % 100_000 == 0 and nonce > 0:
            elapsed = time.time() - t0
            rate = nonce / elapsed if elapsed > 0 else float("inf")
            print(f"  {nonce:,} hashes ({rate:.0f} H/s)…", end="\r", flush=True)

    raise RuntimeError("Exhausted nonce space (0–0xFFFFFFFE) without finding a valid hash")


# ── Chain helpers ─────────────────────────────────────────────────────────────

def get_block_count(host: str, port: int, user: str, password: str) -> int:
    """Return the current chain tip height.  Raises RuntimeError on failure."""
    return rpc_request(host, port, user, password, "getblockcount")

def get_nbits_for_next_block(host: str, port: int, user: str, password: str,
                             prev_block_hash: str,
                             prev_block_height: int,
                             powlimit_override: int | None = None) -> int:
    """
    Return the nBits value that the next block must carry.

    This function implements the testnet difficulty rules:
    1. If --powlimit override is set, use that value.
    2. For blocks with height >= nPowDGWHeight (650), use DGW algorithm
       (we get the value from the previous block header).
    3. For early blocks (height < nPowDGWHeight), the node uses powLimit.
    4. Additionally, testnet min-difficulty rule: if block time > prev_time + 300s,
       the node returns powLimit.GetCompact() instead of previous nBits.

    Since our mining timestamp is always "now" (current system time), which is
    always > 300 seconds after the genesis block (March 2026), the min-difficulty
    rule always applies on testnet.  We therefore use TESTNET_POWLIMIT_NBITS
    (0x1e0fffff) instead of the genesis nBits (0x1e0ffff0).
    """
    if powlimit_override is not None:
        return powlimit_override

    current_time = int(time.time())

    try:
        header_info = rpc_request(
            host, port, user, password,
            "getblockheader", [prev_block_hash]
        )
        prev_time = header_info["time"]
        prev_bits = int(header_info["bits"], 16)
        prev_height = header_info["height"]

        print(f"  [Debug] Prev height: {prev_height}, prev_time: {prev_time}, prev_bits: 0x{prev_bits:08x}")
        print(f"  [Debug] Current time: {current_time}, time gap: {current_time - prev_time}s")

        # Check if we're in the early block phase (before DGW activates)
        # nPowDGWHeight = 650 for testnet
        DGW_ACTIVATION_HEIGHT = 650

        if prev_height + 1 < DGW_ACTIVATION_HEIGHT:
            # Early blocks: node uses powLimit.GetCompact()
            # On testnet, this is 0x1e0fffff (NOT 0x1e0ffff0 from genesis)
            print(f"  [Difficulty] Height {prev_height + 1} < DGW activation ({DGW_ACTIVATION_HEIGHT})")
            print(f"  [Difficulty] Using powLimit: 0x{TESTNET_POWLIMIT_NBITS:08x}")
            return TESTNET_POWLIMIT_NBITS

        # Check testnet min-difficulty rule:
        # If block time > prev_time + 2 * nPowTargetSpacing (300s),
        # the node returns powLimit.GetCompact()
        if current_time > prev_time + TESTNET_MIN_DIFFICULTY_WINDOW:
            print(f"  [Difficulty] Min-difficulty rule active (time gap: {current_time - prev_time}s > {TESTNET_MIN_DIFFICULTY_WINDOW}s)")
            print(f"  [Difficulty] Using powLimit: 0x{TESTNET_POWLIMIT_NBITS:08x}")
            return TESTNET_POWLIMIT_NBITS

        # Normal case: use previous block's nBits
        print(f"  [Difficulty] Using prev block nBits: 0x{prev_bits:08x}")
        return prev_bits

    except Exception as exc:
        print(f"  [Warning] getblockheader failed ({exc}); using powLimit.")
        return TESTNET_POWLIMIT_NBITS


# ── Interactive prompt ────────────────────────────────────────────────────────

def get_input(prompt: str, default=None) -> str:
    # Use `is not None` so that integer/falsy defaults (e.g. 0) are handled.
    if default is not None:
        val = input(f"{prompt} [{default}]: ").strip()
        return val if val else str(default)
    while True:
        val = input(f"{prompt}: ").strip()
        if val:
            return val
        print("  (required)")


# ── CLI ───────────────────────────────────────────────────────────────────────

def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="GoByte block generator — CPU-mines blocks and submits via RPC.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
If the node rejects with "bad-diffbits":
  For blocks 1-23 DGW has fewer than 24 ancestors, so GetNextWorkRequired()
  returns powLimit.GetCompact().  If chainparams.cpp changed testnet powLimit
  during the 0.17 upgrade (e.g. to ~uint256(0)>>1 = 0x207fffff), every early
  block will be rejected with bad-diffbits because we derive nBits from the
  genesis header (0x1e0ffff0), not from powLimit.

  Fix:
    1. grep -A3 "CTestNetParams" src/chainparams.cpp | grep powLimit
    2. python3 -c "
         p = int('<powLimit hex>', 16)
         e = (p.bit_length()+7)//8
         m = (p >> (8*(e-3))) & 0x7fffff
         print(hex((e<<24)|m))"
    3. Rerun:  python3 generate-blocks.py --powlimit <result>
        """,
    )
    parser.add_argument(
        "--powlimit",
        metavar="HEX",
        default=None,
        help=(
            "Override nBits for all mined blocks (hex, e.g. 0x207fffff). "
            "Use when node rejects with bad-diffbits.  See epilog for details."
        ),
    )
    return parser.parse_args()


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    args = _parse_args()

    powlimit_override: int | None = None
    if args.powlimit is not None:
        try:
            powlimit_override = int(args.powlimit, 16)
            print(f"[Config] powlimit override: 0x{powlimit_override:08x}")
        except ValueError:
            sys.exit(f"\nERROR: --powlimit must be a hex value (got {args.powlimit!r})\n")

    print("=" * 60)
    print("  GoByte Block Generator")
    print("  (Works during IBD — no sync required)")
    print("=" * 60)
    print()

    rpcport = int(get_input("RPC Port", 13454))
    rpcuser = get_input("RPC Username")
    rpcpassword = get_input("RPC Password")
    rpchost = get_input("RPC Host", "127.0.0.1")
    mining_address = get_input("Mining Address (G-prefix mainnet or n-prefix testnet)")
    blocks = int(get_input("Number of blocks to mine"))

    # Validate address before doing any work.
    print()
    print("[Address] Validating mining address…")
    try:
        _address_to_p2pkh_script(mining_address)
        print(f"[Address] OK: {mining_address}")
    except ValueError as exc:
        sys.exit(f"\nERROR: Invalid mining address — {exc}\n")

    print()
    print("[NeoScrypt] Loading…")
    neoscrypt = _load_neoscrypt()

    print(f"[RPC] Connecting to {rpchost}:{rpcport}…")

    try:
        current_height = get_block_count(rpchost, rpcport, rpcuser, rpcpassword)
    except RuntimeError as exc:
        sys.exit(f"\nERROR: Cannot reach node — {exc}\n")

    print(f"[Chain] Current height: {current_height}")

    if current_height == 0:
        prev_block = TESTNET_GENESIS["hash"]
        prev_block_height = 0  # Genesis block is at height 0
        start_height = 1
        print(f"[Chain] Starting from genesis → prev={prev_block[:16]}…")
    else:
        try:
            prev_block = rpc_request(
                rpchost, rpcport, rpcuser, rpcpassword,
                "getblockhash", [current_height]
            )
            prev_block_height = current_height
            start_height = current_height + 1
            print(f"[Chain] Continuing from height {current_height}, prev={prev_block[:16]}…")
        except RuntimeError as exc:
            sys.exit(f"\nERROR: Could not fetch tip block hash — {exc}\n")

    print(f"[Mining] Generating {blocks} block(s) starting at height {start_height}…")
    if powlimit_override is not None:
        print(f"[Mining] nBits override: 0x{powlimit_override:08x} (all blocks)")
    print()

    for i in range(blocks):
        height = start_height + i
        print(f"--- Block {i + 1}/{blocks}  (height {height}) ---")

        ntime = int(time.time())

        nbits = get_nbits_for_next_block(
            rpchost, rpcport, rpcuser, rpcpassword,
            prev_block,
            prev_block_height,
            powlimit_override=powlimit_override,
        )

        coinbase_tx = build_coinbase_tx(height, mining_address)
        coinbase_hex = coinbase_tx.hex()

        coinbase_hash = (
            hashlib.sha256(hashlib.sha256(coinbase_tx).digest()).digest()[::-1].hex()
        )

        block_data = {
            "nVersion": 1,
            "hashPrevBlock": prev_block,
            "hashMerkleRoot": coinbase_hash,
            "nTime": ntime,
            "nBits": nbits,
        }

        nonce, block_hash = mine_block(block_data, neoscrypt)

        block_hex = serialize_block(block_data, coinbase_hex, nonce)

        result = rpc_request(
            rpchost, rpcport, rpcuser, rpcpassword,
            "submitblock", [block_hex]
        )

        # submitblock returns null on success; any string is a rejection reason.
        if result is None:
            print(f"  ✓ Accepted: {block_hash}")
            prev_block = block_hash
            prev_block_height = height  # Update for next block
        else:
            print(f"  ✗ Rejected: {result}")
            if result == "bad-diffbits":
                print()
                print("  DIAGNOSTIC: bad-diffbits")
                print(f"  We submitted nBits = 0x{nbits:08x}")
                print("  The node's GetNextWorkRequired() returned a different value.")
                print("  For blocks 1-23 (DGW window not full), this equals")
                print("  powLimit.GetCompact() from CTestNetParams in chainparams.cpp.")
                print()
                print("  To find the correct value:")
                print("    grep -A3 'CTestNetParams\\|TestNet' src/chainparams.cpp | grep powLimit")
                print()
                print("  Then convert to compact nBits and rerun:")
                print("    python3 generate-blocks.py --powlimit 0x<value>")
                print()
                print("  Common values:")
                print("    0x1e0ffff0  (~uint256(0) >> 20, GoByte mainnet default)")
                print("    0x207fffff  (~uint256(0) >>  1, easy testnet/regtest)")
            print("  Aborting.")
            break

        time.sleep(0.5)

    print()
    print("Done.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nAborted.")
        sys.exit(0)
