#include "all.h"

WasmOp wasm_op[NOp] = {
#define O(op, t, x) [O##op] =
#define V(imm) { imm },
#include "../ops.h"
};

/*
 * We don't have real registers, but we need to
 * trick the register allocator into thinking we do.
 *
 * We define two classes of virtual registers,
 * integer and float, and we don't bother with
 * giving them fancy names.
 */

#define N_GPR 16
#define N_FPR 16

static int
wasm_memargs(int op)
{
	(void)op;
	return 0;
}

Target T_wasm = {
	.name = "wasm",
	.gpr0 = Tmp0,
	.ngpr = N_GPR,
	.fpr0 = Tmp0 + N_GPR,
	.nfpr = N_FPR,
	.memargs = wasm_memargs,
	.abi0 = elimsb,
	.abi1 = wasm_abi,
	.isel = wasm_isel,
	.emitfn = wasm_emitfn,
	.emitfin = wasm_emitfin,
};
