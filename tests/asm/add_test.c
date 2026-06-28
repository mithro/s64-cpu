#include <stdio.h>
#include <stdint.h>

#define ENCODE(op,rd,rs1,rs2,imm8,m) \
    ( ((uint32_t)(op)<<24)|((uint32_t)(rd)<<19)|((uint32_t)(rs1)<<14)| \
      ((uint32_t)(rs2)<<9)|((uint32_t)(imm8)<<1)|((uint32_t)(m)) )

int main(void) {
    uint32_t prog[] = {
        ENCODE(0x49, 0, 0, 0, 1, 1),  /* MOVI R0, #1     */
        ENCODE(0x49, 1, 0, 0, 2, 1),  /* MOVI R1, #2     */
        ENCODE(0x00, 2, 0, 1, 0, 0),  /* ADD  R2, R0, R1 */
        ENCODE(0x61, 0, 0, 0, 0, 0),  /* HLT             */
    };
    FILE *f = fopen("tests/asm/add_test.bin", "wb");
    fwrite(prog, 4, 4, f);
    fclose(f);
    printf("wrote add_test.bin\n");
}
