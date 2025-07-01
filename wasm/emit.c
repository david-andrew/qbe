/*
To run this, you need to have a few tools installed:

1. wabt: https://github.com/WebAssembly/wabt
2. wasmtime: https://wasmtime.dev/

Once you have them, you can compile and run the generated .wat file like this:

wat2wasm test.wat -o test.wasm
wasmtime test.wasm --invoke <function_name> <args>

For example:

wasmtime test.wasm --invoke main 1 2

Here is a minimal html harness to run the wasm module in a browser:

<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Wasm Test</title>
</head>
<body>
<script>
(async () => {
  const response = await fetch('test.wasm');
  const buffer = await response.arrayBuffer();
  const { instance } = await WebAssembly.instantiate(buffer);
  const result = instance.exports.main(1, 2);
  console.log(result);
})();
</script>
</body>
</html>

*/

#include "all.h"

static int *tmp_to_wasm_idx; // Map Tmp.val to WebAssembly local index
static int next_wasm_idx;

static void
emitins(Ins *i, Fn *fn, FILE *f)
{
	switch (i->op) {
	case Oadd:
		fprintf(f, "    local.get %d\n", tmp_to_wasm_idx[i->arg[0].val - Tmp0]);
		fprintf(f, "    local.get %d\n", tmp_to_wasm_idx[i->arg[1].val - Tmp0]);
		fprintf(f, "    i32.add\n");
		break;
	case Ocall:
		// Push arguments onto the stack
		// i->arg[0] is the function reference, subsequent args are parameters
		for (int arg_idx = 1; !req(i->arg[arg_idx], R); arg_idx++) {
			if (i->arg[arg_idx].type == RCon) {
				Con *c = &fn->con[i->arg[arg_idx].val];
				if (c->type == CBits) {
					fprintf(f, "    i32.const %d\n", (int)c->bits.i);
				}
			} else if (i->arg[arg_idx].type == RTmp) {
				// This case might not be hit for constants, but for temporaries
				fprintf(f, "    local.get %d\n", tmp_to_wasm_idx[i->arg[arg_idx].val - Tmp0]);
			}
		}
		// Call the function
		// The function name is in fn->tmp[i->arg[0].val].name
		fprintf(f, "    call $%s\n", fn->tmp[i->arg[0].val].name);
		// Store the result of the call in the destination temporary
		if (i->to.type == RTmp) {
			fprintf(f, "    local.set %d\n", tmp_to_wasm_idx[i->to.val - Tmp0]);
		}
		break;
	default:
		break;
	}
}

void
wasm_emitfn(Fn *fn, FILE *f)
{
	Blk *b;
	Ins *i;
	uint n;

	// Allocate tmp_to_wasm_idx dynamically based on fn->ntmp
	tmp_to_wasm_idx = vnew(fn->ntmp - Tmp0, sizeof(int), PFn);

	// Reset WebAssembly local index counter for each function
	next_wasm_idx = 0;

	fprintf(f, "(module\n");
	fprintf(f, "  (func $%s (export \"%s\"))", fn->name, fn->name);

	// Emit parameters and build tmp_to_wasm_idx map
	for (n = Tmp0; n < fn->ntmp; n++) {
		Tmp *t = &fn->tmp[n];
		// A Tmp is a parameter if its defining instruction is Opar
		if (t->def != NULL && (t->def->op == Opar || t->def->op == Oparc || t->def->op == Opare || (t->def->op >= Oparsb && t->def->op <= Oparuh))) {
			fprintf(f, " (param $%s i32)", t->name);
			tmp_to_wasm_idx[n - Tmp0] = next_wasm_idx++;
		} else {
			// Initialize to -1 for non-parameters
			tmp_to_wasm_idx[n - Tmp0] = -1;
		}
	}

	// Now emit locals for temporaries that are not parameters
	for (n = Tmp0; n < fn->ntmp; n++) {
		Tmp *t = &fn->tmp[n];
		if (tmp_to_wasm_idx[n - Tmp0] == -1) { // If not already assigned as a parameter
			// Check if this temporary is actually used and needs a local
			// This is a simplification; a proper liveness analysis would be better
			if (t->def != NULL) { // If it has a defining instruction, it's a local
				fprintf(f, " (local $%s i32)", t->name);
				tmp_to_wasm_idx[n - Tmp0] = next_wasm_idx++;
			}
		}
	}

	fprintf(f, " (result i32)\n");

	for (b = fn->start; b; b = b->link) {
		for (i = b->ins; i < &b->ins[b->nins]; i++)
			emitins(i, fn, f);
		if (isret(b->jmp.type)) {
			if (b->jmp.arg.type == RCon) {
				Con *c = &fn->con[b->jmp.arg.val];
				if (c->type == CBits) {
					fprintf(f, "    i32.const %d\n", (int)c->bits.i);
				}
			} else if (b->jmp.arg.type == RTmp) {
				fprintf(f, "    local.get %d\n", tmp_to_wasm_idx[b->jmp.arg.val - Tmp0]);
			}
			fprintf(f, "    return\n");
		}
	}

	fprintf(f, "  )\n");
	fprintf(f, ")\n");
}

void
wasm_emitfin(FILE *f)
{
	(void)f;
}
