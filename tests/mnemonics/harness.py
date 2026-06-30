"""
Shared harness for the as64 mnemonic test suite.

Drives the real `as` (as64) binary end-to-end: writes a .s64 source string to
a temp file, assembles it with --flat, and returns the raw instruction
bytes/words so tests can assert on the *exact* encoded bits, plus
stdout/stderr/returncode so tests can assert on diagnostics.

No third-party dependencies (no pytest) -- everything here is stdlib only,
so the suite runs anywhere Python 3 + the project's Makefile/gcc run.
"""
import os
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
AS_BIN = os.path.join(REPO_ROOT, "as")


def ensure_built():
    """Build the assembler if it isn't already present (or is stale)."""
    need_build = not os.path.isfile(AS_BIN)
    if not need_build:
        # rebuild if any as64/*.c is newer than the binary
        as_mtime = os.path.getmtime(AS_BIN)
        src_dir = os.path.join(REPO_ROOT, "as64")
        for fn in os.listdir(src_dir):
            if fn.endswith(".c") or fn.endswith(".h"):
                if os.path.getmtime(os.path.join(src_dir, fn)) > as_mtime:
                    need_build = True
                    break
    if need_build:
        r = subprocess.run(["make", "as64"], cwd=REPO_ROOT,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if r.returncode != 0 or not os.path.isfile(AS_BIN):
            sys.stderr.write(r.stdout.decode())
            sys.stderr.write(r.stderr.decode())
            raise RuntimeError("failed to build as64 ('as' binary)")


class AsmResult:
    def __init__(self, returncode, stdout, stderr, flat_bytes):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.flat_bytes = flat_bytes

    @property
    def ok(self):
        return self.returncode == 0

    @property
    def words(self):
        """Decode flat_bytes into a list of little-endian 32-bit words."""
        b = self.flat_bytes or b""
        assert len(b) % 4 == 0, f"output not word-aligned: {len(b)} bytes"
        out = []
        for i in range(0, len(b), 4):
            w = b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (b[i + 3] << 24)
            out.append(w)
        return out


def assemble(src, extra_args=None):
    """Assemble `src` (a .s64 source string) to a flat binary and return
    an AsmResult. Always uses --flat so we can directly compare raw
    instruction words without parsing the .o64/SXF container format."""
    ensure_built()
    extra_args = extra_args or []
    with tempfile.TemporaryDirectory() as d:
        src_path = os.path.join(d, "test.s64")
        out_path = os.path.join(d, "test.bin")
        with open(src_path, "w") as f:
            f.write(src)
        args = [AS_BIN, src_path, "--flat", "-o", out_path] + extra_args
        r = subprocess.run(args, cwd=d, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
        flat = None
        if os.path.isfile(out_path):
            with open(out_path, "rb") as f:
                flat = f.read()
        return AsmResult(r.returncode, r.stdout.decode(errors="replace"),
                          r.stderr.decode(errors="replace"), flat)


def enc(op, rd=0, rs1=0, rs2=0, imm8=0, m=0):
    """Python re-implementation of ENCODE_INSTR from include/s64.h, used to
    compute the *expected* instruction word for comparison."""
    op &= 0xFF
    rd &= 0x1F
    rs1 &= 0x1F
    rs2 &= 0x1F
    imm8 &= 0xFF
    m &= 0x1
    return (op << 24) | (rd << 19) | (rs1 << 14) | (rs2 << 9) | (imm8 << 1) | m


def hexw(w):
    return f"0x{w:08X}"
