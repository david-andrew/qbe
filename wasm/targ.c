#include "all.h"

static int
wasm_memargs(int op)
{
	(void)op;
	return 0;
}

static void
wasm_abi0(Fn *fn)
{
	(void)fn;
}

Target T_wasm = {
	.name = "wasm",
	.gpr0 = 0,
	.ngpr = 0,
	.fpr0 = 0,
	.nfpr = 0,
	.rglob = 0,
	.nrglob = 0,
	.rsave = {0},
	.nrsave = {0, 0},
	.retregs = 0,
	.argregs = 0,
	.memargs = wasm_memargs,
	.abi0 = wasm_abi0,
	.abi1 = wasm_abi,
	.isel = wasm_isel,
	.emitfn = wasm_emitfn,
	.emitfin = wasm_emitfin,
	.asloc = "$",
};
