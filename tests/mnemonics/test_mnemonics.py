"""
test_mnemonics.py -- exercises every mnemonic in as64/encoder.c's opcode
table with its valid syntax form(s) and checks the *exact* encoded
instruction word against a from-scratch Python re-implementation of
ENCODE_INSTR (see harness.enc).

Run:
    cd s64-cpu-master && python3 -m unittest discover -s tests/mnemonics -v
or:
    python3 tests/mnemonics/test_mnemonics.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))
from harness import assemble, enc, hexw  # noqa: E402

# opcode values, mirrored from include/s64.h (kept independent of the C
# source on purpose -- if someone changes s64.h without updating the
# assembler or vice versa, these tests should catch the drift)
OP = dict(
    SUB=0x01, MUL=0x02, DIV=0x03, MOD=0x04, ADDI=0x05, SUBI=0x06, MULI=0x07,
    ADD=0x08,
    AND=0x10, OR=0x11, XOR=0x12, NOT=0x13, SHL=0x14, SHR=0x15, SAR=0x16,
    SHLI=0x17, SHRI=0x18,
    EQ=0x20, NE=0x21, LT=0x22, GT=0x23, LTE=0x24, GTE=0x25, LTU=0x26, GTU=0x27,
    JMP=0x30, JMPI=0x31, JEQ=0x32, JNE=0x33, CALL=0x34, RET=0x35, LOOP=0x36,
    LD64=0x40, LD32=0x41, LD16=0x42, LD8=0x43,
    ST64=0x44, ST32=0x45, ST16=0x46, ST8=0x47,
    MOV=0x48, MOVI=0x49, MOVW=0x4A,
    FADD=0x50, FSUB=0x51, FMUL=0x52, FDIV=0x53, FCMP=0x54, FTOI=0x55, ITOF=0x56,
    NOP=0x60, HLT=0x61, SYS=0x62, PUSH=0x63, POP=0x64, TRAP=0x65, SRET=0x66,
    SETPRIV=0x67,
)


def asm_words(src):
    r = assemble(src)
    if not r.ok:
        raise AssertionError(f"assembly failed for {src!r}:\n{r.stderr}")
    return r.words


class Base(unittest.TestCase):
    def assertEncodes(self, src, expected_word, msg=None):
        words = asm_words(src)
        self.assertEqual(len(words), 1, f"expected exactly 1 word for {src!r}, got {words}")
        self.assertEqual(words[0], expected_word,
                          f"{src!r}: got {hexw(words[0])}, want {hexw(expected_word)}" +
                          (f" ({msg})" if msg else ""))

    def assertAsmFails(self, src, needle=None):
        r = assemble(src)
        self.assertFalse(r.ok, f"expected assembly to fail for {src!r}, but it succeeded")
        if needle:
            self.assertIn(needle, r.stderr,
                           f"{src!r}: stderr {r.stderr!r} did not contain {needle!r}")


# ─── 3-register / 3-reg-or-imm ALU group: ADD SUB MUL SHL SHR + their *I forms ──

class ThreeOperandGroup(Base):
    """ADD, SUB, MUL, SHL, SHR (and the dedicated ADDI/SUBI/MULI/SHLI/SHRI
    opcodes) all share one encoder switch-case: operand 3 may be EITHER a
    register (M=0) OR an immediate (M=1), auto-detected from what's parsed.
    This holds true even for the '*I' mnemonics -- the `force_m` field in
    encoder.c's opcode_table is declared but never consulted by
    encoder_encode(), so e.g. `ADDI Rd, Rs1, Rs2` (a register operand) is
    accepted and silently encodes as register mode despite the mnemonic
    implying immediate-only. This suite documents that as actual,
    verified behavior."""

    MNEMONICS = ["ADD", "SUB", "MUL", "SHL", "SHR",
                 "ADDI", "SUBI", "MULI", "SHLI", "SHRI"]

    def test_register_mode(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R2, R3, R4\n",
                                    enc(OP[m], rd=2, rs1=3, rs2=4, imm8=0, m=0))

    def test_immediate_mode(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R2, R3, #7\n",
                                    enc(OP[m], rd=2, rs1=3, imm8=7, m=1))

    def test_immediate_mode_even_for_I_suffixed_mnemonic_with_register_operand(self):
        # documents the force_m dead-code quirk described above
        for m in ["ADDI", "SUBI", "MULI", "SHLI", "SHRI"]:
            base = m[:-1]  # ADD, SUB, MUL, SHL, SHR
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R1, R2, R3\n",
                                    enc(OP[m], rd=1, rs1=2, rs2=3, imm8=0, m=0),
                                    msg="force_m is declared but unused by encoder_encode()")

    def test_immediate_boundary_high_255_ok(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R0, R1, #255\n",
                                    enc(OP[m], rd=0, rs1=1, imm8=255, m=1))

    def test_immediate_boundary_256_rejected(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R0, R1, #256\n", "out of range")

    def test_immediate_boundary_low_neg128_ok(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                # -128 encodes as raw byte 0x80 (two's complement)
                self.assertEncodes(f"{m} R0, R1, #-128\n",
                                    enc(OP[m], rd=0, rs1=1, imm8=0x80, m=1))

    def test_immediate_boundary_neg129_rejected(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R0, R1, #-129\n", "out of range")

    def test_immediate_zero(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R0, R1, #0\n",
                                    enc(OP[m], rd=0, rs1=1, imm8=0, m=1))

    def test_hex_and_hex_negative_immediates(self):
        self.assertEncodes("ADD R0, R1, #0x1F\n", enc(OP["ADD"], 0, 1, 0, 0x1F, 1))

    def test_missing_third_operand_errors(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R0, R1\n", "requires 3 operand")

    def test_missing_all_operands_errors(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m}\n", "requires 3 operand")

    def test_register_zero_zero_zero(self):
        self.assertEncodes("ADD R0, R0, R0\n", enc(OP["ADD"], 0, 0, 0, 0, 0))

    def test_max_register_numbers(self):
        self.assertEncodes("ADD R31, R31, R31\n", enc(OP["ADD"], 31, 31, 31, 0, 0))


# ─── 2-register-only ALU group: must NOT accept an immediate operand 3 ────

class TwoRegOnlyGroup(Base):
    MNEMONICS = ["DIV", "MOD", "AND", "OR", "XOR", "SAR",
                 "EQ", "NE", "LT", "GT", "LTE", "GTE", "LTU", "GTU",
                 "FADD", "FSUB", "FMUL", "FDIV", "FCMP"]

    def test_register_mode(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R5, R6, R7\n",
                                    enc(OP[m], rd=5, rs1=6, rs2=7, imm8=0, m=0))

    def test_immediate_operand_rejected(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R0, R1, #5\n", "requires register as operand 3")

    def test_missing_operands(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R0, R1\n", "requires 3 operand")


# ─── unary register ops: NOT, FTOI, ITOF (Rd, Rs1) ────────────────────────

class UnaryRegOps(Base):
    MNEMONICS = ["NOT", "FTOI", "ITOF"]

    def test_encoding(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R3, R9\n",
                                    enc(OP[m], rd=3, rs1=9, rs2=0, imm8=0, m=0))

    def test_missing_operand(self):
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R3\n", "requires 2 operand")

    def test_extra_third_operand_is_ignored(self):
        # encoder only reads ops[0]/ops[1]; a 3rd operand is harmlessly parsed
        # but never consulted
        for m in self.MNEMONICS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R3, R9, R20\n",
                                    enc(OP[m], rd=3, rs1=9, rs2=0, imm8=0, m=0))


# ─── MOV / MOVI / MOVW ─────────────────────────────────────────────────────

class MovFamily(Base):
    def test_mov_reg_to_reg(self):
        self.assertEncodes("MOV R1, R2\n", enc(OP["MOV"], rd=1, rs1=2, m=0))

    def test_mov_missing_operand(self):
        self.assertAsmFails("MOV R1\n", "requires 2 operand")

    def test_movi_basic(self):
        self.assertEncodes("MOVI R0, #1\n", enc(OP["MOVI"], rd=0, imm8=1, m=1))

    def test_movi_doc_example_hex(self):
        # cross-check against docs/S64.md section 9.4 worked example
        self.assertEncodes("MOVI R0, #1\n", 0x49000003)

    def test_movi_imm8_truncates_silently_out_of_8bit_range(self):
        # MOVI's IMM1 macro just masks with 0xFF -- unlike the 3-operand ALU
        # group, there is NO range check here, so out-of-range immediates
        # are silently truncated rather than rejected. This documents that
        # real (if surprising) behavior: 300 & 0xFF == 44.
        r = assemble("MOVI R0, #300\n")
        self.assertTrue(r.ok)
        self.assertEqual(r.words[0], enc(OP["MOVI"], rd=0, imm8=300 & 0xFF, m=1))

    def test_movi_negative_truncates(self):
        r = assemble("MOVI R0, #-1\n")
        self.assertTrue(r.ok)
        self.assertEqual(r.words[0], enc(OP["MOVI"], rd=0, imm8=0xFF, m=1))

    def test_movw_basic(self):
        self.assertEncodes("MOVW R5, #0xAB\n", enc(OP["MOVW"], rd=5, imm8=0xAB, m=1))


# ─── control flow: JMP JMPI JEQ JNE CALL RET LOOP ─────────────────────────

class ControlFlow(Base):
    def test_jmp(self):
        self.assertEncodes("JMP R5\n", enc(OP["JMP"], rd=0, rs1=5, m=0))

    def test_jmp_missing_operand(self):
        self.assertAsmFails("JMP\n", "requires 1 operand")

    def test_jmpi(self):
        self.assertEncodes("JMPI #10\n", enc(OP["JMPI"], imm8=10, m=1))

    def test_jmpi_negative(self):
        self.assertEncodes("JMPI #-3\n", enc(OP["JMPI"], imm8=0xFD, m=1))

    def test_jeq(self):
        self.assertEncodes("JEQ R5, R3\n", enc(OP["JEQ"], rs1=5, rs2=3, m=0))

    def test_jne(self):
        self.assertEncodes("JNE R5, R3\n", enc(OP["JNE"], rs1=5, rs2=3, m=0))

    def test_jeq_missing_operand(self):
        self.assertAsmFails("JEQ R5\n", "requires 2 operand")

    def test_call(self):
        self.assertEncodes("CALL R30, R5\n", enc(OP["CALL"], rd=30, rs1=5, m=0))

    def test_call_missing_operand(self):
        self.assertAsmFails("CALL R30\n", "requires 2 operand")

    def test_ret(self):
        self.assertEncodes("RET R30\n", enc(OP["RET"], rs1=30, m=0))
        self.assertEncodes("RET LR\n", enc(OP["RET"], rs1=30, m=0))  # LR alias == R30

    def test_ret_missing_operand(self):
        self.assertAsmFails("RET\n", "requires 1 operand")

    def test_loop(self):
        self.assertEncodes("LOOP R4, R7\n", enc(OP["LOOP"], rd=4, rs1=7, m=0))


# ─── memory: LD8/16/32/64, ST8/16/32/64 ────────────────────────────────────

class MemoryOps(Base):
    LOADS = ["LD64", "LD32", "LD16", "LD8"]
    STORES = ["ST64", "ST32", "ST16", "ST8"]

    def test_loads_with_offset(self):
        for m in self.LOADS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R0, [R1 #8]\n",
                                    enc(OP[m], rd=0, rs1=1, imm8=8, m=1))

    def test_loads_no_offset_defaults_to_zero(self):
        for m in self.LOADS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R0, [R1]\n",
                                    enc(OP[m], rd=0, rs1=1, imm8=0, m=1))

    def test_loads_negative_offset(self):
        for m in self.LOADS:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R0, [R1 #-5]\n",
                                    enc(OP[m], rd=0, rs1=1, imm8=0xFB, m=1))

    def test_loads_comma_before_offset_is_a_syntax_error(self):
        # the bracket-operand grammar in parse_operand() treats the '+' as
        # optional but does NOT accept a comma between the base register and
        # the offset -- "[R1, #8]" is rejected, only "[R1 #8]" / "[R1 8]" work
        for m in self.LOADS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R0, [R1, #8]\n", "RBRACKET")

    def test_loads_require_mem_operand(self):
        for m in self.LOADS:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R0, R1\n", "requires [Rs + #off]")

    def test_stores_value_then_mem(self):
        # ST<n> Rs1(value), [Rs2(base) + #off]; encoder maps RD->value reg
        for m in self.STORES:
            with self.subTest(mnem=m):
                self.assertEncodes(f"{m} R2, [R3 #4]\n",
                                    enc(OP[m], rd=2, rs1=0, rs2=3, imm8=4, m=1))

    def test_stores_require_mem_operand(self):
        for m in self.STORES:
            with self.subTest(mnem=m):
                self.assertAsmFails(f"{m} R2, R3\n", "requires [Rs + #off]")


# ─── system / misc: NOP HLT SYS PUSH POP TRAP SRET SETPRIV ───────────────

class SystemMisc(Base):
    def test_nop(self):
        self.assertEncodes("NOP\n", enc(OP["NOP"]))

    def test_hlt(self):
        self.assertEncodes("HLT\n", enc(OP["HLT"]))

    def test_nop_hlt_ignore_extra_operands_if_any_parse(self):
        # NOP/HLT's encoder case takes no operands from `ins` at all, so even
        # if someone (incorrectly) supplies operands, the encoded word is
        # unaffected -- only opcode/0s come out.
        self.assertEncodes("NOP R0, R1\n", enc(OP["NOP"]))

    def test_sys(self):
        self.assertEncodes("SYS #5\n", enc(OP["SYS"], imm8=5, m=1))

    def test_sys_missing_operand(self):
        self.assertAsmFails("SYS\n", "requires 1 operand")

    def test_trap(self):
        self.assertEncodes("TRAP #1\n", enc(OP["TRAP"], imm8=1, m=1))

    def test_sret_no_operands(self):
        self.assertEncodes("SRET\n", enc(OP["SRET"]))

    def test_setpriv(self):
        self.assertEncodes("SETPRIV R5, #1\n",
                            enc(OP["SETPRIV"], rs1=5, imm8=1, m=1))

    def test_setpriv_requires_register_first_operand(self):
        self.assertAsmFails("SETPRIV #1, #2\n", "SETPRIV requires Rs1, #imm")

    def test_pop(self):
        self.assertEncodes("POP R4, R28\n", enc(OP["POP"], rd=4, rs1=28, m=0))

    def test_pop_missing_operand(self):
        self.assertAsmFails("POP R4\n", "requires 2 operand")

    def test_push_two_operand_form_drops_the_value_register(self):
        # KNOWN BUG, captured here on purpose: the PUSH encoder case reads
        # operand[1] as Rs1 and operand[2] as Rs2 (`encode(op, 0, RS1, RS2,
        # 0, 0)`), but REQ_OPS(2) only guarantees ops[0] and ops[1] exist.
        # With the documented 2-operand syntax "PUSH Rs, SP", ops[2] is left
        # zeroed by the Instruction memset, so the value register the user
        # actually wrote (ops[0]) is silently discarded and Rs2 always comes
        # out as 0/R0, regardless of what was typed as the second operand.
        r = assemble("PUSH R5, R28\n")
        self.assertTrue(r.ok)
        # rs1 ends up holding what the user wrote as the *second* operand
        # (R28), and rs2 is always 0 -- NOT R5 anywhere in the encoding.
        self.assertEqual(r.words[0], enc(OP["PUSH"], rd=0, rs1=28, rs2=0, m=0))

    def test_push_three_operand_form_is_the_one_that_actually_works(self):
        # supplying a (throwaway) first operand shifts the real Rs1/Rs2 into
        # ops[1]/ops[2] where the encoder actually reads them
        r = assemble("PUSH R0, R5, R28\n")
        self.assertTrue(r.ok)
        self.assertEqual(r.words[0], enc(OP["PUSH"], rd=0, rs1=5, rs2=28, m=0))

    def test_push_missing_operands(self):
        self.assertAsmFails("PUSH R5\n", "requires 2 operand")


# ─── LA pseudo-op (parser-level special case, not in encoder.c table) ─────

class LaPseudoOp(Base):
    def test_la_emits_fixed_4_word_chain(self):
        words = asm_words("LA R3, target\ntarget:\n")
        self.assertEqual(len(words), 4)
        self.assertEqual(words[0], enc(OP["MOVI"], rd=3, imm8=0, m=1))
        for w in words[1:]:
            self.assertEqual(w, enc(OP["MOVW"], rd=3, imm8=0, m=1))

    def test_la_requires_register_and_label(self):
        r = assemble("LA #1, target\ntarget:\n")
        self.assertFalse(r.ok)
        self.assertIn("LA requires Rd, label", r.stderr)

    def test_la_with_undefined_label_still_emits_chain(self):
        # LA always defers to a link-time relocation (RELOC_WIDE), so even an
        # undefined/forward label produces the same fixed 16-byte chain
        # rather than an assembler error.
        words = asm_words("LA R0, never_defined\n")
        self.assertEqual(len(words), 4)


# ─── unknown mnemonics / malformed lines ───────────────────────────────────

class UnknownAndMalformed(Base):
    def test_unknown_mnemonic(self):
        self.assertAsmFails("FROBNICATE R0\n", "unknown mnemonic")

    def test_typo_close_to_real_mnemonic(self):
        self.assertAsmFails("ADX R0, R1, R2\n", "unknown mnemonic")

    def test_register_number_31_is_max_32_is_not_a_register(self):
        # R32 doesn't parse as a register (parse_reg only accepts 0-31), so
        # the lexer hands it back as a plain identifier -> "unknown mnemonic"
        # when used in mnemonic position, since it can't be a register here
        self.assertAsmFails("R32 R0, R1, R2\n", "unknown mnemonic")

    def test_empty_program_assembles_to_nothing(self):
        r = assemble("; just a comment\n")
        self.assertTrue(r.ok)
        self.assertEqual(r.flat_bytes, b"")

    def test_bare_label_no_instruction(self):
        r = assemble("start:\n")
        self.assertTrue(r.ok)
        self.assertEqual(r.flat_bytes, b"")


if __name__ == "__main__":
    unittest.main()
