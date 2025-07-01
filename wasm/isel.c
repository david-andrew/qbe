#include "all.h"

static void
sel_cmp(Ins i, Fn *fn)
{
	char *s;
	int ck, cc;

	iscmp(i.op, &ck, &cc);

	if (ck == Kw || ck == Kl) {
		switch (cc) {
		case Cieq: s = "i32.eq"; break;
		case Cine: s = "i32.ne"; break;
		case Cisle: s = "i32.le_s"; break;
		case Cislt: s = "i32.lt_s"; break;
		case Cisge: s = "i32.ge_s"; break;
		case Cisgt: s = "i32.gt_s"; break;
		case Ciule: s = "i32.le_u"; break;
		case Ciult: s = "i32.lt_u"; break;
		case Ciuge: s = "i32.ge_u"; break;
		case Ciugt: s = "i32.gt_u"; break;
		default: die("unhandled comparison");
		}
	} else {
		switch (cc) {
		case Cfeq: s = "f32.eq"; break;
		case Cfne: s = "f32.ne"; break;
		case Cfle: s = "f32.le"; break;
		case Cflt: s = "f32.lt"; break;
		case Cfge: s = "f32.ge"; break;
		case Cfgt: s = "f32.gt"; break;
		default: die("unhandled comparison");
		}
	}

	emit(Ocmp, ck, i.to, i.arg[0], i.arg[1]);
	curi->s = s;
}

static void
sel_ins(Ins i, Fn *fn)
{
	if (iscmp(i.op, 0, 0)) {
		sel_cmp(i, fn);
		return;
	}
	emiti(i);
}

void
wasm_isel(Fn *fn)
{
	Blk *b;
	Ins *i;

	for (b=fn->start; b; b=b->link) {
		curi = &insb[NIns];
		for (i=&b->ins[b->nins]; i!=b->ins;)
			sel_ins(*--i, fn);
		idup(b, curi, &insb[NIns]-curi);
	}

	if (debug['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}