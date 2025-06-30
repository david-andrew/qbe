#include "all.h"

/* In Wasm, all computed values are on the stack.
 * There are no complex addressing modes. This function
 * just ensures that non-register temporaries are loaded.
 */
static void
fixarg(Ref *r, int k, Fn *fn)
{
	if (rtype(*r) == RTmp && !isreg(*r)) {
		if (fn->tmp[r->val].slot != -1) {
			/* promoted stack slot, load its address */
			Ref r_addr = newtmp("isel", Kl, fn);
			emit(Oaddr, Kl, r_addr, SLOT(fn->tmp[r->val].slot), R);
			*r = r_addr;
		}
	}
	(void)k;
}

static void
selcmp(Ins i, int k, int cc)
{
	int op;

	if (KBASE(k) == 0) { /* Integer comparisons */
		switch (cc) {
		case Cieq:  op = Oieq; break;
		case Cine:  op = Oine; break;
		case Cisge: op = Oisge; break;
		case Cisgt: op = Oisgt; break;
		case Cisle: op = Oisle; break;
		case Cislt: op = Oislt; break;
		case Ciuge: op = Oiuge; break;
		case Ciugt: op = Oiugt; break;
		case Ciule: op = Oiule; break;
		case Ciult: op = Oiult; break;
		default:
			die("unhandled integer comparison %d", cc);
			return;
		}
	} else { /* Floating point comparisons */
		switch (cc) {
		case Cfeq: op = Ofeq; break;
		case Cfne: op = Ofne; break;
		case Cfge: op = Ofge; break;
		case Cfgt: op = Ofgt; break;
		case Cfle: op = Ofle; break;
		case Cflt: op = Oflt; break;
		/* Ordered/Unordered require instruction decomposition.
		 * Deferring for now. */
		case Cfo:
		case Cfuo:
		default:
			die("unhandled float comparison %d", cc);
			return;
		}
	}

	emit(op, k, i.to, i.arg[0], i.arg[1]);
}

static void
sel(Ins i, Fn *fn)
{
	int ck, cc;

	if (isstore(i.op)) {
		fixarg(&i.arg[1], Kl, fn);
	} else if (isload(i.op)) {
		fixarg(&i.arg[0], Kl, fn);
	}

	if (iscmp(i.op, &ck, &cc)) {
		selcmp(i, ck, cc);
		return;
	}

	if (i.op == Oalloc) {
		salloc(i.to, i.arg[0], fn);
		return;
	}

	emiti(i);
}

static void
seljmp(Blk *b, Fn *fn)
{
	if (b->jmp.type == Jjnz)
		fixarg(&b->jmp.arg, Kw, fn);
}

void
wasm_isel(Fn *fn)
{
	Blk *b;
	Ins *i;
	int s;

	fn->slot = 0;
	/* align stack to 16 bytes */
	fn->salign = 4;

	/* assign stack slots for `alloca` */
	for (b=fn->start; b; b=b->link) {
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			if (i->op != Oalloc)
				continue;
			if (rtype(i->arg[0]) != RCon)
				err("non-constant alloca not supported");
			s = fn->con[i->arg[0].val].bits.i;
			if (s < 0)
				err("invalid alloca size");

			fn->slot += (s + 15) & ~15;
			fn->tmp[i->to.val].slot = fn->slot;
			*i = (Ins){.op = Onop};
		}
	}


	for (b = fn->start; b; b = b->link) {
		curi = &insb[NIns];
		for (i = &b->ins[b->nins]; i != b->ins;)
			sel(*--i, fn);
		seljmp(b, fn);
		idup(b, curi, &insb[NIns] - curi);
	}

	if (debug['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}