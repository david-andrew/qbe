#include "../all.h"

/* wasm/abi.c */
void wasm_abi(Fn *);

/* wasm/isel.c */
void wasm_isel(Fn *);

/* wasm/emit.c */
void wasm_emitfn(Fn *, FILE *);
void wasm_emitfin(FILE *);