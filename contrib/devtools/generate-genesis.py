#!/usr/bin/env python3
"""
generate-genesis.py — Genesis block miner for GoByte / Dash-derivative chains.

Run from anywhere inside the repository:
    python3 contrib/devtools/generate-genesis.py

No external dependencies required.  The script locates neoscrypt.cpp in
src/crypto/ and compiles it on the fly.  If a pre-installed neoscrypt
Python package is present it will be used instead (faster startup).

Requires: a C/C++ compiler on PATH (clang, gcc, or cl.exe).
          On macOS this is always available via Xcode Command Line Tools.
          On Linux: apt install build-essential
"""

from __future__ import annotations   # allows X | Y type hints on Python 3.9

import ctypes
import datetime
import hashlib
import os
import platform
import struct
import subprocess
import sys
import tempfile
import time


# ── NeoScrypt loader ────────────────────────────────────────────────────────

# Known-good smoke-test vector (regtest genesis):
#   nTime=1510727100, nNonce=901219, nBits=0x1e0ffff0
# Built with struct.pack to avoid byte-transcription errors.
_SMOKE_MERKLE_DISPLAY = (
    "dc9a719dc1bcda39107ea55424f00cab512170a1cb69efa08531f483f2399f21"
)
_SMOKE_HEADER = (
    struct.pack("<I", 1)                                     # nVersion
    + bytes(32)                                              # hashPrevBlock (null)
    + bytes.fromhex(_SMOKE_MERKLE_DISPLAY)[::-1]             # hashMerkleRoot (LE)
    + struct.pack("<I", 1510727100)                          # nTime
    + struct.pack("<I", 0x1E0FFFF0)                          # nBits
    + struct.pack("<I", 901219)                              # nNonce
)
_SMOKE_EXPECTED = "00000dbc9aa1686b4dfb177300185c6a3e0b13d1d4d346c5bccdd19fdf9ebc5a"


def _verify_fn(fn) -> bool:
    """Return True iff fn produces the correct regtest genesis hash."""
    try:
        result = fn(_SMOKE_HEADER)[::-1].hex()
        return result == _SMOKE_EXPECTED
    except Exception:
        return False


def _load_so(path: str):
    """
    Load a shared library that exports `neoscrypt(in, out, profile)`.
    Returns a verified callable or None.
    """
    try:
        lib = ctypes.CDLL(path)
        lib.neoscrypt.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32]
        lib.neoscrypt.restype  = None
        _out = ctypes.create_string_buffer(32)

        def fn(header: bytes) -> bytes:
            lib.neoscrypt(header, _out, 0x0)
            return bytes(_out)

        return fn if _verify_fn(fn) else None
    except Exception:
        return None


def _try_pip_package():
    """
    Look for a pre-installed neoscrypt Python package and extract its .so.
    Returns a verified callable or None.
    """
    import importlib.util, glob, site

    if importlib.util.find_spec("neoscrypt") is None:
        return None

    try:
        dirs = site.getsitepackages() + [site.getusersitepackages()]
    except AttributeError:
        dirs = [site.getusersitepackages()]

    for d in dirs:
        for pat in ("neoscrypt*.so", "neoscrypt*.pyd", "neoscrypt*.dylib"):
            for path in glob.glob(os.path.join(d, pat)):
                fn = _load_so(path)
                if fn is not None:
                    return fn
    return None


def _find_repo_source() -> str | None:
    """
    Walk up from this script's location looking for the GoByte repo root.
    Returns the path to neoscrypt.c/.cpp, or None.
    """
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
    """
    Compile neoscrypt.c/.cpp into a temporary shared library.
    Tries C compilation first (guarantees C linkage), then C++.
    Returns a verified callable or None.
    """
    src_dir  = os.path.dirname(src_file)
    system   = platform.system()
    lib_name = (
        "neoscrypt_genesis.dylib" if system == "Darwin" else "neoscrypt_genesis.so"
    )
    lib_path = os.path.join(tempfile.gettempdir(), lib_name)

    # -dynamiclib on macOS, -shared elsewhere
    shared_flag = "-dynamiclib" if system == "Darwin" else "-shared"

    # Try C compilers first — preserves C linkage (no name mangling)
    # Then fall back to C++ compilers (works if neoscrypt.h has extern "C")
    compiler_tries = [
        (["clang", "gcc", "cc"],          ["-x", "c"]),
        (["clang++", "g++", "c++"],       []),
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
    """
    Return a callable  hash_fn(header: bytes) -> bytes[32]  that computes
    NeoScrypt with profile 0x0 (GoByte PoW).  Resolution order:

        1. Pre-installed neoscrypt Python package
        2. Auto-compile from src/crypto/neoscrypt.cpp in the repository
        3. Fatal error with clear instructions
    """
    fn = _try_pip_package()
    if fn is not None:
        return fn

    src = _find_repo_source()
    if src is not None:
        print(f"  [neoscrypt] Compiling from {os.path.relpath(src)} …", flush=True)
        fn = _compile_from_source(src)
        if fn is not None:
            print("  [neoscrypt] Ready.\n", flush=True)
            return fn
        print("  [neoscrypt] Compilation failed.\n", flush=True)

    sys.exit(
        "\nERROR: Could not load NeoScrypt.\n\n"
        "Options:\n"
        "  a) Run this script from inside the GoByte repository tree so it\n"
        "     can find and compile src/crypto/neoscrypt.cpp automatically.\n\n"
        "  b) Install the Python package (any Python version):\n"
        "       pip install neoscrypt\n"
    )


# ── Bitcoin/Dash serialization primitives ──────────────────────────────────

def _dsha256(data: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def _encode_script_num(n: int) -> bytes:
    """CScriptNum sign-magnitude little-endian encoding."""
    if n == 0:
        return b""
    result = bytearray()
    neg    = n < 0
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


def _encode_varint(n: int) -> bytes:
    if n < 0xFD:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xFD" + struct.pack("<H", n)
    if n <= 0xFFFFFFFF:
        return b"\xFE" + struct.pack("<I", n)
    return b"\xFF" + struct.pack("<Q", n)


# ── Coinbase transaction builder ────────────────────────────────────────────

def build_coinbase_tx(
    timestamp_str: str,
    pubkey_hex: str,
    reward_satoshis: int,
) -> bytes:
    """
    Reproduces exactly what CreateGenesisBlock() does in chainparams.cpp:

        txNew.vin[0].scriptSig = CScript()
            << 486604799
            << CScriptNum(4)
            << std::vector<unsigned char>(pszTimestamp…);
        txNew.vout[0].nValue       = genesisReward;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex(pubkey) << OP_CHECKSIG;
    """
    pszTimestamp  = timestamp_str.encode("utf-8")
    pub_key       = bytes.fromhex(pubkey_hex)
    output_script = _push_data(pub_key) + bytes([0xAC])  # OP_CHECKSIG

    script_sig = (
        _push_data(_encode_script_num(486604799))
        + _push_data(_encode_script_num(4))
        + _push_data(pszTimestamp)
    )
    vin = (
        bytes(32)
        + struct.pack("<I", 0xFFFFFFFF)
        + _encode_varint(len(script_sig)) + script_sig
        + struct.pack("<I", 0xFFFFFFFF)
    )
    vout = (
        struct.pack("<q", reward_satoshis)
        + _encode_varint(len(output_script)) + output_script
    )
    return (
        struct.pack("<i", 1)
        + _encode_varint(1) + vin
        + _encode_varint(1) + vout
        + struct.pack("<I", 0)
    )


# ── nBits → target ──────────────────────────────────────────────────────────

def _nbits_to_target(nbits: int) -> int:
    exp      = (nbits >> 24) & 0xFF
    mantissa = nbits & 0x7FFFFF
    return mantissa * (1 << (8 * (exp - 3)))


# ── Interactive prompt helpers ──────────────────────────────────────────────

def _prompt(label: str, default=None, convert=str):
    suffix = f" [{default}]" if default is not None else ""
    while True:
        raw = input(f"  {label}{suffix}: ").strip()
        if raw == "" and default is not None:
            return default
        if raw == "":
            print("    (required — please enter a value)")
            continue
        try:
            return convert(raw)
        except (ValueError, TypeError) as exc:
            print(f"    Invalid: {exc}")


def _prompt_date(label: str, default_ts: int | None = None):
    """Accept a Unix timestamp or YYYY-MM-DD string; return int timestamp."""
    default_str = None
    if default_ts is not None:
        dt = datetime.datetime.fromtimestamp(default_ts, tz=datetime.timezone.utc)
        default_str = dt.strftime("%Y-%m-%d")

    suffix = f" [{default_str} = {default_ts}]" if default_str else ""
    print(f"  {label}{suffix}")
    print("    (Unix timestamp  OR  YYYY-MM-DD for midnight UTC)")

    while True:
        raw = input("  > ").strip()
        if raw == "" and default_ts is not None:
            return default_ts
        if raw == "":
            print("    (required)")
            continue
        try:
            return int(raw)
        except ValueError:
            pass
        try:
            dt = datetime.datetime.strptime(raw, "%Y-%m-%d").replace(
                tzinfo=datetime.timezone.utc
            )
            ts = int(dt.timestamp())
            print(f"    → {ts}  ({dt.strftime('%Y-%m-%d %H:%M:%S UTC')})")
            return ts
        except ValueError:
            print("    Unrecognised — use a Unix timestamp or YYYY-MM-DD")


def _prompt_hex(
    label: str,
    default: str | None = None,
    expected_bytes: int | None = None,
):
    suffix = f" [{default[:16]}…]" if default else ""
    while True:
        raw = input(f"  {label}{suffix}: ").strip()
        if raw.startswith(("0x", "0X")):
            raw = raw[2:]
        if raw == "" and default is not None:
            return default
        if raw == "":
            print("    (required)")
            continue
        try:
            b = bytes.fromhex(raw)
        except ValueError:
            print("    Not valid hex.")
            continue
        if expected_bytes and len(b) != expected_bytes:
            print(
                f"    Expected {expected_bytes} bytes "
                f"({expected_bytes * 2} hex chars), got {len(b)}."
            )
            continue
        return raw


def _prompt_int_hex(label: str, default_hex: str | None = None):
    """Accept decimal int or 0x-prefixed hex; return int."""
    while True:
        raw = input(f"  {label} [{default_hex}]: ").strip()
        if raw == "" and default_hex is not None:
            return int(default_hex, 16) if default_hex.startswith("0x") else int(default_hex)
        try:
            return int(raw, 0)
        except ValueError:
            print("    Enter a decimal integer or 0x-prefixed hex (e.g. 0x1e0ffff0)")


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    print("=" * 70)
    print("  GoByte / Dash-derivative Genesis Block Generator")
    print("=" * 70)
    print()
    print("Press Enter to accept the [default] value shown in brackets.")
    print()

    DEFAULT_TIMESTAMP = (
        "The Star Malaysia 17th November 2017 GoByte Genesis Reborn"
    )
    DEFAULT_PUBKEY = (
        "043e5a5fbfbb2caa5f4b7c8fd24d890d6c244de254d579b5ba629f64c1b48275f"
        "59e0e1c834a60f6ffb4aaa022aaa4866434ca729a12465f80618fb2070045cb16"
    )

    # ── Step 1: coinbase ───────────────────────────────────────────────────
    print("── Step 1: Coinbase transaction ──────────────────────────────────")
    pszTimestamp = _prompt(
        "Timestamp string (pszTimestamp)",
        default=DEFAULT_TIMESTAMP,
    )
    pubkey_hex = _prompt_hex(
        "Genesis output pubkey (uncompressed, 65 bytes = 130 hex chars)",
        default=DEFAULT_PUBKEY,
        expected_bytes=65,
    )
    reward_coins = _prompt("Block reward in whole coins", default=50, convert=int)
    reward_satoshis = reward_coins * 100_000_000
    print()

    # ── Step 2: block header ───────────────────────────────────────────────
    print("── Step 2: Block header ──────────────────────────────────────────")
    ntime = _prompt_date("Genesis date (nTime)", default_ts=1774483200)
    nbits = _prompt_int_hex("nBits (compact target)", default_hex="0x1e0ffff0")
    nver  = _prompt("Block version (nVersion)", default=1, convert=int)
    print()

    # ── Load NeoScrypt ─────────────────────────────────────────────────────
    print("── Loading NeoScrypt ─────────────────────────────────────────────")
    neoscrypt = _load_neoscrypt()

    # ── Compute merkle root ────────────────────────────────────────────────
    print("── Computing merkle root from coinbase …")
    tx      = build_coinbase_tx(pszTimestamp, pubkey_hex, reward_satoshis)
    txid_le = _dsha256(tx)
    merkle  = txid_le[::-1].hex()
    print(f"   hashMerkleRoot = {merkle}")
    print()

    # ── Mine ───────────────────────────────────────────────────────────────
    target = _nbits_to_target(nbits)
    print(f"── Mining (nBits={nbits:#010x}) ──────────────────────────────────")
    print(f"   target = {target:#066x}")
    print()

    header_prefix = (
        struct.pack("<I", nver)
        + bytes(32)
        + txid_le
        + struct.pack("<I", ntime)
        + struct.pack("<I", nbits)
    )

    t0 = time.time()
    found_nonce = None
    found_hash  = None

    for nonce in range(0, 0xFFFFFFFF):
        header = header_prefix + struct.pack("<I", nonce)
        raw    = neoscrypt(header)
        if int.from_bytes(raw, "little") <= target:
            found_nonce = nonce
            found_hash  = raw[::-1].hex()
            break
        if nonce % 200_000 == 0 and nonce:
            elapsed = time.time() - t0
            print(
                f"   {nonce:>10,} hashes  ({nonce / elapsed:,.0f} h/s) …",
                end="\r",
                flush=True,
            )

    if found_nonce is None:
        sys.exit("\nERROR: Exhausted all 2^32 nonces without a valid hash.")

    elapsed = time.time() - t0
    print(f"   Found valid nonce in {elapsed:.1f}s                              ")
    print()

    # ── Results ────────────────────────────────────────────────────────────
    dt_str = datetime.datetime.fromtimestamp(
        ntime, tz=datetime.timezone.utc
    ).strftime("%B %d, %Y")

    print("=" * 70)
    print("  RESULTS")
    print("=" * 70)
    print()
    print(f"  nTime            : {ntime}  ({dt_str})")
    print(f"  nNonce           : {found_nonce}")
    print(f"  nBits            : {nbits:#010x}")
    print(f"  nVersion         : {nver}")
    print(f"  hashMerkleRoot   : {merkle}")
    print(f"  hashGenesisBlock : {found_hash}")
    print()
    print("── chainparams.cpp ───────────────────────────────────────────────")
    print()
    print(
        f"        genesis = CreateGenesisBlock({ntime}, {found_nonce}, "
        f"{nbits:#010x}, {nver}, {reward_coins} * COIN); "
        f"// last reset: {dt_str}"
    )
    print( "        consensus.hashGenesisBlock = genesis.GetHash();")
    print(f"        assert(consensus.hashGenesisBlock == uint256S(\"0x{found_hash}\"));")
    print(f"        assert(genesis.hashMerkleRoot == uint256S(\"0x{merkle}\"));")
    print()
    print("── checkpoint {0} ────────────────────────────────────────────────")
    print()
    print(f"                {{0, uint256S(\"0x{found_hash}\")}},")
    print()
    print("── chainTxData ───────────────────────────────────────────────────")
    print()
    print( "        chainTxData = ChainTxData{")
    print(f"            {ntime}, // genesis timestamp ({dt_str})")
    print( "            1,       // genesis tx only")
    print( "            0        // no rate data yet; update after first real traffic")
    print( "        };")
    print()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nAborted.")
        sys.exit(0)
