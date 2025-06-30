#include "all.h"

/*
 * WebAssembly ABI is simpler than traditional ABIs.
 * Arguments are passed as function parameters.
 * Return values are function results.
 * QBE's `Opar` and `Oarg` are translated directly.
 *
 * This implementation is basic and does not yet handle
 * struct passing/returning by value. They should be passed
 * by pointer.
 */

bits
wasm_retregs(Ref r, int p[2])
{
	(void)r; (void)p;
	return 0;
}

bits
wasm_argregs(Ref r, int p[2])
{
	(void)r; (void)p;
	return 0;
}

static void
selret(Blk *b, Fn *fn)
{
	if (!isret(b->jmp.type) || b->jmp.type == Jret0)
		return;

	if (b->jmp.type == Jretc)
		err("passing structs by value not supported yet");

	/* The 'return' instruction in wasm will take the
	   value from the stack. The emit phase will ensure
	   the correct value is on the stack. */
	emit(Ocopy, b->jmp.type - Jretw, R, b->jmp.arg, R);
	b->jmp.type = Jret0;
}

static void
selcall(Fn *fn, Ins *i0, Ins *i1)
{
	Ins *i;
	int n;
	Ref r;

	/* move arguments onto wasm stack */
	for (i=i0, n=0; i<i1; i++) {
		if (i->op == Oargc)
			err("passing structs by value not supported yet");
		if (isarg(i->op)) {
			emit(Ocopy, i->cls, R, i->arg[0], R);
			n++;
		}
	}
	
	r = CALL(n); /* pass arg count in call val */
	emit(Ocall, 0, i1->to, i1->arg[0], r);

	/* if the call has a return value, it will be on the
	   stack. the copy will be handled by the caller. */
	if (!req(i1->to, R)) {
		emit(Ocopy, i1->cls, i1->to, R, R);
	}
}

static void
selpar(Fn *fn, Ins *i0, Ins *i1)
{
	Ins *i;
	int n;

	for (i=i0, n=0; i<i1; i++) {
		if (i->op == Oparc)
			err("passing structs by value not supported yet");
		if (ispar(i->op)) {
			emit(Ocopy, i->cls, i->to, INT(n++), R);
		}
	}
}

void
wasm_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0;
	int n;

	for (b = fn->start; b; b = b->link) {
		curi = &insb[NIns];
		selret(b, fn);
		for (i = &b->ins[b->nins]; i != b->ins;) {
			--i;
			if (i->op == Ocall) {
				for (i0 = i; i0 > b->ins; i0--)
					if (!isarg((i0-1)->op))
						break;
				selcall(fn, i0, i);
				i = i0;
			} else if (isarg(i->op)) {
				/* arguments are processed in selcall */
			} else {
				emiti(*i);
			}
		}
		idup(b, curi, &insb[NIns]-curi);
	}

	b = fn->start;
	for (i = b->ins; i < &b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	n = i - b->ins;
	curi = &insb[NIns];
	selpar(fn, b->ins, i);

	vgrow(&b->ins, b->nins - n + (&insb[NIns] - curi));
	memmove(b->ins + (&insb[NIns] - curi), i, (b->nins - n) * sizeof(Ins));
	memcpy(b->ins, curi, (&insb[NIns] - curi) * sizeof(Ins));
	b->nins += (&insb[NIns] - curi) - n;

	if (debug['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}