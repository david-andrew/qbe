#include "../all.h"

typedef struct WasmOp WasmOp;

/*
 * WebAssembly does not have a fixed set of general-purpose registers.
 * It uses a value stack and typed local variables. For QBE's register
 * allocator, we define a set of virtual registers. These will be mapped
 * to Wasm locals during code emission.
 */
enum WasmReg {
	/* Virtual integer registers (i32/i64) */
	I0 = RXX + 1, I1, I2, I3, I4, I5, I6, I7,
	I8, I9, I10, I11, I12, I13, I14, I15,

	/* Virtual floating-point registers (f32/f64) */
	F0, F1, F2, F3, F4, F5, F6, F7,
	F8, F9, F10, F11, F12, F13, F14, F15,

	NGPR = 16, /* Number of virtual integer registers */
	NFPR = 16, /* Number of virtual float registers */
};
MAKESURE(reg_not_tmp, F15 < (int)Tmp0);

/*
 * We define new wasm-specific opcodes here.
 * The instruction selector will generate these,
 * and the emitter will know how to print them.
 */
enum {
	Oieq = NOp, Oine,
	Oislt, Oisle, Oisgt, Oisge,
	Oiult, Oiule, Oiugt, Oiuge,
	Ofeq, Ofne,
	Oflt, Ofle, Ofgt, Ofge,
};

struct WasmOp {
	char imm;
};

/* targ.c */
extern WasmOp wasm_op[NOp];

/* abi.c */
bits wasm_retregs(Ref, int[2]);
bits wasm_argregs(Ref, int[2]);
void wasm_abi(Fn *);

/* isel.c */
void wasm_isel(Fn *);

/* emit.c */
void wasm_emitfn(Fn *, FILE *);
void wasm_emitfin(FILE *);