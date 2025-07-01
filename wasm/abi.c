#include "all.h"

/* The Wasm ABI.
 *
 * This ABI is very simple.
 * Arguments are passed on the value stack.
 * Return values are also passed on the value stack.
 * Large structs are passed by pointer.
 * There is no support for varargs.
 */

typedef struct Insl Insl;

struct Insl {
	Ins i;
	Insl *link;
};

static void
typclass(Typ *t, int *cls, int *is_ptr)
{
	*is_ptr = 0;
	if (t->isdark || t->size > 16 || t->size == 0) {
		*is_ptr = 1;
		*cls = Kl;
	} else {
		/* For now, we only handle simple types.
		 * Structs will be implemented later.
		 */
		switch (t->size) {
		case 1: *cls = Kw; break;
		case 2: *cls = Kw; break;
		case 4: *cls = Kw; break;
		case 8: *cls = Kl; break;
		default:
			if (t->isunion)
				err("unions not supported yet");
			else
				err("structs not supported yet");
		}
	}
}

static void
selret(Blk *b, Fn *fn)
{
	int j, k, is_ptr;
	Ref r;
	int cls;
	j = b->jmp.type;
	if (!isret(j) || j == Jret0)
		return;
	r = b->jmp.arg;
	b->jmp.type = Jret0;
	if (j == Jretc) {
		typclass(&typ[fn->retty], &cls, &is_ptr);
		if (is_ptr) {
			assert(rtype(fn->retr) == RTmp);
			emit(Oblit1, 0, R, INT(typ[fn->retty].size), R);
			emit(Oblit0, 0, R, r, fn->retr);
		} else {
			emit(Ocopy, cls, R, r, R);
		}
	} else {
		k = j - Jretw;
		emit(Ocopy, k, R, r, R);
	}
	b->jmp.arg = R; /* no call info needed */
}

static void
stkblob(Ref r, Typ *t, Fn *fn, Insl **ilp)
{
	Insl *il;
	int al;
	uint64_t sz;
	il = alloc(sizeof *il);
	al = t->align - 2; /* specific to NAlign == 3 */
	if (al < 0)
		al = 0;
	sz = (t->size + 7) & ~7;
	il->i = (Ins){Oalloc+al, Kl, r, {getcon(sz, fn)}};
	il->link = *ilp;
	*ilp = il;
}

static void
selcall(Fn *fn, Ins *i0, Ins *i1, Insl **ilp)
{
	Ins *i;
	int cls, is_ptr;
	Typ *t;
	for (i=i0; i<i1; i++) {
		switch (i->op) {
		case Oarg:
			emit(Ocopy, i->cls, R, i->arg[0], R);
			break;
		case Oargc:
			t = &typ[i->arg[0].val];
			typclass(t, &cls, &is_ptr);
			if (is_ptr) {
				Ref ptr = newtmp("abi", Kl, fn);
				stkblob(ptr, t, fn, ilp);
				emit(Oblit1, 0, R, INT(t->size), R);
				emit(Oblit0, 0, R, i->arg[1], ptr);
				emit(Ocopy, Kl, R, ptr, R);
			} else {
				emit(Oload, cls, R, i->arg[1], R);
			}
			break;
		case Oarge:
		case Opare:
			/* env pointer, not supported */
			break;
		case Oargv:
			die("vastart not supported in wasm");
		}
	}

	if (!req(i1->arg[1], R)) {
		t = &typ[i1->arg[1].val];
		typclass(t, &cls, &is_ptr);
		if (is_ptr) {
			stkblob(i1->to, t, fn, ilp);
			emit(Ocopy, Kl, R, i1->to, R);
		}
	}

	emit(Ocall, 0, R, i1->arg[0], R);

	if (!req(i1->arg[1], R)) {
		t = &typ[i1->arg[1].val];
		typclass(t, &cls, &is_ptr);
		if (is_ptr) {
			/* result is a pointer, it's already in i1->to */
		} else {
			emit(Ocopy, cls, i1->to, R, R);
		}
	} else {
		emit(Ocopy, i1->cls, i1->to, R, R);
	}
}

static void
selpar(Fn *fn, Ins *i0, Ins *i1)
{
	Ins *i;
	int cls, is_ptr;
	Typ *t;
	curi = &insb[NIns];
	if (fn->retty >= 0) {
		t = &typ[fn->retty];
		typclass(t, &cls, &is_ptr);
		if (is_ptr) {
			fn->retr = newtmp("abi", Kl, fn);
			emit(Opar, Kl, fn->retr, R, R);
		}
	}

	for (i=i0; i<i1; i++) {
		switch (i->op) {
		case Opar:
			emit(Ocopy, i->cls, i->to, R, R);
			break;
		case Oparc:
			t = &typ[i->arg[0].val];
			typclass(t, &cls, &is_ptr);
			if (is_ptr) {
				Ref ptr = newtmp("abi", Kl, fn);
				emit(Opar, Kl, ptr, R, R);
				emit(Oblit1, 0, R, INT(t->size), R);
				emit(Oblit0, 0, R, i->to, ptr);
			} else {
				emit(Ostorel, cls, R, i->to, R);
			}
			break;
		}
	}
}

void
wasm_abi(Fn *fn)
{
	Blk *b;
	Ins *i, *i0;
	Insl *il;
	int n0, n1, ioff;
	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	/* lower parameters */
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	selpar(fn, b->ins, i);
	n0 = &insb[NIns] - curi;
	ioff = i - b->ins;
	n1 = b->nins - ioff;
	vgrow(&b->ins, n0+n1);
	icpy(b->ins+n0, b->ins+ioff, n1);
	icpy(b->ins, curi, n0);
	b->nins = n0+n1;

	/* lower calls and returns */
	il = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start; /* do it last */
		if (b->visit)
			continue;
		curi = &insb[NIns];
		selret(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;) {
			--i;
			if (i->op == Ocall) {
				for (i0=i; i0>b->ins; i0--)
					if (!isarg((i0-1)->op))
						break;
				selcall(fn, i0, i, &il);
				i = i0;
			} else if (i->op == Ovastart || i->op == Ovaarg) {
				die("vastart/vaarg not supported in wasm");
			} else if (!isarg(i->op)) {
				emiti(*i);
			}
		}
		if (b == fn->start)
			for (; il; il=il->link)
				emiti(il->i);
		idup(b, curi, &insb[NIns]-curi);
	} while (b != fn->start);

	if (debug['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}