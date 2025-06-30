#include "all.h"

WasmOp wasm_op[NOp] = {
#define O(op, t, x) [O##op] =
#define V(imm) { imm },
#include "../ops.h"
};

/* All virtual registers are caller-saved as Wasm functions have their own scope */
static int wasm_rsave[] = {
	I0, I1, I2, I3, I4, I5, I6, I7,
	I8, I9, I10, I11, I12, I13, I14, I15,
	F0, F1, F2, F3, F4, F5, F6, F7,
	F8, F9, F10, F11, F12, F13, F14, F15,
	-1
};

/* No callee-saved registers in the traditional sense */
static int wasm_rclob[] = { -1 };

static int
wasm_memargs(int op)
{
	(void)op;
	return 0; /* Wasm loads from a value on the stack, not complex addressing modes */
}

Target T_wasm = {
	.name = "wasm",
	.gpr0 = I0,
	.ngpr = NGPR,
	.fpr0 = F0,
	.nfpr = NFPR,
	.rglob = 0, /* No global registers like SP/FP */
	.nrglob = 0,
	.rsave = wasm_rsave,
	.nrsave = {NGPR, NFPR},
	.retregs = wasm_retregs,
	.argregs = wasm_argregs,
	.memargs = wasm_memargs,
	.abi0 = elimsb,
	.abi1 = wasm_abi,
	.isel = wasm_isel,
	.emitfn = wasm_emitfn,
	.emitfin = wasm_emitfin,
	.asloc = "$L",
	.assym = "$",
};

MAKESURE(rsave_size_ok, sizeof wasm_rsave == (NGPR + NFPR + 1) * sizeof(int));