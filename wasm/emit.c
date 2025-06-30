#include "all.h"
#include <ctype.h>

static char *wop[NOp+32] = { /* Increased size for wasm opcodes */
	[Oadd] = "add", [Osub] = "sub", [Omul] = "mul",
	[Odiv] = "div_s", [Oudiv] = "div_u",
	[Orem] = "rem_s", [Ourem] = "rem_u",
	[Oand] = "and", [Oor] = "or", [Oxor] = "xor",
	[Oshl] = "shl", [Oshr] = "shr_u", [Osar] = "shr_s",
	[Oload] = "load", [Oloadsb] = "load8_s", [Oloadub] = "load8_u",
	[Oloadsh] = "load16_s", [Oloaduh] = "load16_u", [Oloadsw] = "load32_s",
	[Ostoreb] = "store8", [Ostoreh] = "store16", [Ostorew] = "store",
	[Ostorel] = "store",  [Ostores] = "store",  [Ostored] = "store",
	[Oextsw] = "extend32_s", [Oexts] = "promote_f32", [Otruncd] = "demote_f64",
	[Oswtof] = "convert_i32_s", [Ouwtof] = "convert_i32_u",
	[Osltof] = "convert_i64_s", [Oultof] = "convert_i64_u",
	[Ostosi] = "trunc_f32_s", [Odtosi] = "trunc_f64_s",
	[Ostoui] = "trunc_f32_u", [Odtoui] = "trunc_f64_u",
	[Ocast] = "reinterpret",
	/* wasm-specific opcodes */
	[Oieq] = "eq", [Oine] = "ne",
	[Oislt] = "lt_s", [Oisle] = "le_s", [Oisgt] = "gt_s", [Oisge] = "ge_s",
	[Oiult] = "lt_u", [Oiule] = "le_u", [Oiugt] = "gt_u", [Oiuge] = "ge_u",
	[Ofeq] = "eq", [Ofne] = "ne",
	[Oflt] = "lt", [Ofle] = "le", [Ofgt] = "gt", [Ofge] = "ge",
};

static char *wty[] = {
	[Kw] = "i32", [Kl] = "i64", [Ks] = "f32", [Kd] = "f64",
};

static void emit_val(Ref r, Fn *fn, FILE *f) {
	switch (rtype(r)) {
	case RCon: {
		Con *c = &fn->con[r.val];
		switch (c->type) {
		case CAddr:
			fprintf(f, "(i32.const %"PRIi64") ;; %s", c->bits.i, str(c->sym.id));
			break;
		case CBits:
			if (c->flt) {
				fprintf(f, "(f%d.const %f)", c->flt == 1 ? 32 : 64, c->flt == 1 ? c->bits.s : c->bits.d);
			} else {
				fprintf(f, "(i64.const %"PRIi64")", c->bits.i);
			}
			break;
		default:
			err("unknown constant type");
		}
		break;
	}
	case RTmp:
		fprintf(f, "(local.get $%s)", fn->tmp[r.val].name);
		break;
	case RInt: /* ABI-specific: param index */
		fprintf(f, "(local.get %d)", r.val);
		break;
	default:
		err("invalid value reference");
	}
}

static void emit_store(Ins *i, Fn *fn, FILE *f) {
	char *op_str = wop[i->op];
	char *ty_str;

	if (!op_str)
		err("unsupported store op %s", optab[i->op].name);

	/* For Wasm store instructions, the address is pushed first, then the value. */
	emit_val(i->arg[1], fn, f); /* address */
	fputc('\n', f);
	emit_val(i->arg[0], fn, f); /* value */
	fputc('\n', f);

	switch (i->op) {
	case Ostoreb:
	case Ostoreh:
	case Ostorew: ty_str = "i32"; break;
	case Ostorel: ty_str = "i64"; break;
	case Ostores: ty_str = "f32"; break;
	case Ostored: ty_str = "f64"; break;
	default: die("unreachable"); /* should be caught by !op_str */
	}

	fprintf(f, "\t(%s.%s)\n", ty_str, op_str);
}

static void emit_ins(Ins *i, Fn *fn, FILE *f) {
	if (isstore(i->op)) {
		emit_store(i, fn, f);
		return;
	}

	if (!req(i->to, R)) {
		fprintf(f, "(local.set $%s\n", fn->tmp[i->to.val].name);
	}

	if (!req(i->arg[0], R)) {
		emit_val(i->arg[0], fn, f);
		fputc('\n', f);
	}
	if (!req(i->arg[1], R)) {
		emit_val(i->arg[1], fn, f);
		fputc('\n', f);
	}

	switch (i->op) {
	case Ocopy:
		/* already handled by emit_val and local.set */
		break;
	case Ocall: {
		Ref r = i->arg[0];
		if (rtype(r) == RCon) {
			Con *c = &fn->con[r.val];
			fprintf(f, "\t(call $%s)\n", str(c->sym.id));
		} else {
			/* indirect call */
			fprintf(f, "\t(call_indirect (type %d) ", i->arg[1].val); /* HACK: use arg1 for type */
			emit_val(r, fn, f);
			fprintf(f,")\n");
		}
		break;
	}
	case Oaddr:
		fprintf(f, "\t(i32.const %d)\n", -fn->tmp[i->arg[0].val].slot);
		fprintf(f, "\t(i32.add (global.get $__stack_pointer))\n");
		break;
	case Onop:
		break;
	default: {
		char *opstr = wop[i->op];
		if (opstr) {
			char *ty = wty[i->cls];
			if (i->op == Ocast) {
				char *ty_from = wty[argcls(i,0)];
				fprintf(f, "\t(%s.%s_%s)\n", ty, opstr, ty_from);
			} else {
				fprintf(f, "\t(%s.%s)\n", ty, opstr);
			}
		} else {
			err("unsupported instruction %s", optab[i->op].name);
		}
		break;
	}
	}

	if (!req(i->to, R)) {
		fprintf(f, ")\n");
	}
}

static void print_harness(FILE *f) {
	fprintf(f, "\n\n");
	fprintf(f, ";; To compile: wat2wasm <this_file.wat> -o module.wasm\n");
	fprintf(f, ";; To run, create an HTML file with the following content:\n\n");
	fprintf(f,
		";; <html>\n"
		";;   <head><meta charset=\"utf-8\"/></head>\n"
		";;   <body>\n"
		";;     <script>\n"
		";;       const memory = new WebAssembly.Memory({ initial: 1 });\n"
		";;       const imports = {\n"
		";;         env: {\n"
		";;           memory: memory,\n"
		";;           print_i32: (arg) => console.log('print_i32:', arg),\n"
		";;           print_i64: (arg) => console.log('print_i64:', arg),\n"
		";;           print_f32: (arg) => console.log('print_f32:', arg),\n"
		";;           print_f64: (arg) => console.log('print_f64:', arg),\n"
		";;         }\n"
		";;       };\n"
		";;       WebAssembly.instantiateStreaming(fetch('module.wasm'), imports)\n"
		";;         .then(obj => {\n"
		";;           const exports = obj.instance.exports;\n"
		";;           console.log('Running _start...');\n"
		";;           const result = exports._start();\n"
		";;           console.log('_start returned:', result);\n"
		";;         });\n"
		";;     </script>\n"
		";;   </body>\n"
		";; </html>\n"
	);
}

void wasm_emitfn(Fn *fn, FILE *f) {
	Blk *b;
	Ins *i;
	Tmp *t;
	int j;

	fprintf(f, "(func $%s", fn->name);

	/* parameters */
	for (i = fn->start->ins; i < &fn->start->ins[fn->start->nins]; i++) {
		if (!ispar(i->op)) break;
		fprintf(f, " (param %s)", wty[i->cls]);
	}

	/* result */
	if (fn->retty != -1 && !req(fn->retr, R)) {
		fprintf(f, " (result %s)", wty[fn->tmp[fn->retr.val].cls]);
	}
	fprintf(f, "\n");

	/* locals */
	for (j=0, t=fn->tmp; j<fn->ntmp; j++, t++) {
		if (t->slot != -1 || t->name[0] == 0) continue; /* stack slots or params */
		if (isreg(TMP(j))) continue;
		fprintf(f, "\t(local $%s %s)\n", t->name, wty[t->cls]);
	}
	fprintf(f, "\t(local $__pc i32)\n");

	/* function body */
	fprintf(f, "\t(local.set $__pc (i32.const %d)) ;; start at block %d\n", fn->start->id, fn->start->id);
	fprintf(f, "\t(loop $__dispatch_loop\n");
	fprintf(f, "\t\t(br_table\n");
	for (b=fn->start; b; b=b->link) {
		fprintf(f, "\t\t\t$L%d\n", b->id);
	}
	fprintf(f, "\t\t\t(local.get $__pc)\n");
	fprintf(f, "\t\t)\n");

	for (b=fn->start; b; b=b->link) {
		fprintf(f, "\t\t(block $L%d\n", b->id);
		for (i=b->ins; i<&b->ins[b->nins]; i++) {
			fprintf(f, "\t\t\t");
			emit_ins(i, fn, f);
		}

		switch (b->jmp.type) {
		case Jret0:
			if (!req(b->jmp.arg, R)) {
				emit_val(b->jmp.arg, fn, f);
				fputc('\n', f);
			}
			fprintf(f, "\t\t\t(return)\n");
			break;
		case Jjmp:
			fprintf(f, "\t\t\t(local.set $__pc (i32.const %d))\n", b->s1->id);
			fprintf(f, "\t\t\t(br $__dispatch_loop)\n");
			break;
		case Jjnz:
			emit_val(b->jmp.arg, fn, f);
			fputc('\n', f);
			fprintf(f, "\t\t\t(if (result i32)\n");
			fprintf(f, "\t\t\t\t(then (i32.const %d)) ;; br to s1 if non-zero\n", b->s1->id);
			fprintf(f, "\t\t\t\t(else (i32.const %d)) ;; br to s2 if zero\n", b->s2->id);
			fprintf(f, "\t\t\t)\n");
			fprintf(f, "\t\t\t(local.set $__pc)\n");
			fprintf(f, "\t\t\t(br $__dispatch_loop)\n");
			break;
		case Jhlt:
			fprintf(f, "\t\t\t(unreachable)\n");
			break;
		}
		fprintf(f, "\t\t)\n");
	}
	fprintf(f, "\t)\n");
	fprintf(f, ")\n\n");
}

void wasm_emitfin(FILE *f) {
	fprintf(f, "(module\n");
	fprintf(f, "\t(import \"env\" \"print_i32\" (func $print_i32 (param i32)))\n");
	fprintf(f, "\t(import \"env\" \"print_i64\" (func $print_i64 (param i64)))\n");
	fprintf(f, "\t(import \"env\" \"print_f32\" (func $print_f32 (param f32)))\n");
	fprintf(f, "\t(import \"env\" \"print_f64\" (func $print_f64 (param f64)))\n");
	fprintf(f, "\t(import \"env\" \"memory\" (memory 1))\n");

	/* A global for the stack pointer, required for alloca */
	fprintf(f, "\t(global $__stack_pointer (mut i32) (i32.const 65536))\n");
	
	/* TODO: Insert all functions here */
	
	fprintf(f, "\t(export \"_start\" (func $_start))\n");

	/* A simple _start function to test with */
	fprintf(f, "\t(func $_start (result i32) (call $main))\n");

	print_harness(f);
}