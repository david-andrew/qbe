#include "../all.h"

typedef struct WasmOp WasmOp;

struct WasmOp {
	char imm;
};

/* targ.c */
extern WasmOp wasm_op[];

/* abi.c */
void wasm_abi(Fn *);

/* isel.c */
void wasm_isel(Fn *);

/* emit.c */
void wasm_emitfn(Fn *, FILE *);
void wasm_emitfin(FILE *);
