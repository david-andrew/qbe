#include "all.h"

static int
isint(int cls)
{
	return cls == Kw || cls == Kl;
}

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
emit_instr(Ins *i, Fn *fn, FILE *f)
{
	switch (i->op) {
	case Oadd:
		if (isint(i->cls))
			fprintf(f, "    i%d.add\n", 32 << (i->cls == Kl));
		else
			fprintf(f, "    f%d.add\n", 32 << (i->cls == Kd));
		break;
	case Osub:
		if (isint(i->cls))
			fprintf(f, "    i%d.sub\n", 32 << (i->cls == Kl));
		else
			fprintf(f, "    f%d.sub\n", 32 << (i->cls == Kd));
		break;
	case Omul:
		if (isint(i->cls))
			fprintf(f, "    i%d.mul\n", 32 << (i->cls == Kl));
		else
			fprintf(f, "    f%d.mul\n", 32 << (i->cls == Kd));
		break;
	case Odiv:
		if (isint(i->cls))
			fprintf(f, "    i%d.div_s\n", 32 << (i->cls == Kl));
		else
			fprintf(f, "    f%d.div\n", 32 << (i->cls == Kd));
		break;
	case Orem:
		fprintf(f, "    i%d.rem_s\n", 32 << (i->cls == Kl));
		break;
	case Oudiv:
		fprintf(f, "    i%d.div_u\n", 32 << (i->cls == Kl));
		break;
	case Ourem:
		fprintf(f, "    i%d.rem_u\n", 32 << (i->cls == Kl));
		break;
	case Oand:
		fprintf(f, "    i%d.and\n", 32 << (i->cls == Kl));
		break;
	case Oor:
		fprintf(f, "    i%d.or\n", 32 << (i->cls == Kl));
		break;
	case Oxor:
		fprintf(f, "    i%d.xor\n", 32 << (i->cls == Kl));
		break;
	case Oshl:
		fprintf(f, "    i%d.shl\n", 32 << (i->cls == Kl));
		break;
	case Oshr:
		fprintf(f, "    i%d.shr_u\n", 32 << (i->cls == Kl));
		break;
	case Osar:
		fprintf(f, "    i%d.shr_s\n", 32 << (i->cls == Kl));
		break;
	case Ocopy:
		if (req(i->to, R))
			fprintf(f, "    drop\n");
		else
			fprintf(f, "    local.set %d\n", i->to.val);
		break;
	case Oload:
		fprintf(f, "    i%d.load\n", 32 << (i->cls == Kl));
		break;
	case Ostorel:
		fprintf(f, "    i%d.store\n", 32 << (i->cls == Kl));
		break;
	case Ocall:
		fprintf(f, "    call %d\n", fn->con[i->arg[0].val].sym.id);
		break;
	case Oaddr:
		fprintf(f, "    global.get %d\n", fn->con[i->arg[0].val].sym.id);
		break;
	case Onop:
		break;
	default:
		fprintf(f, "    unimplemented %s\n", optab[i->op].name);
		break;
	}
}

static void
emit_blit(FILE *f)
{
	fprintf(f,
		"  (func $blit (param $dst i32) (param $src i32) (param $sz i32)\n"
		"    (loop $blit_loop\n"
		"      (if (i32.eqz (local.get $sz))\n"
		"        (then\n"
		"          (return)\n"
		"        )\n"
		"      )\n"
		"      (local.set $sz (i32.sub (local.get $sz) (i32.const 1)))\n"
		"      (i32.store8\n"
		"        (i32.add (local.get $dst) (local.get $sz))\n"
		"        (i32.load8_u (i32.add (local.get $src) (local.get $sz)))\n"
		"      )\n"
		"      (br $blit_loop)\n"
		"    )\n"
		"  )\n"
	);
}

void
wasm_emitfn(Fn *fn, FILE *f)
{
	Blk *b;
	Ins *i;
	int j, cls, is_ptr;

	fprintf(f, "  (func $%s", fn->name);
	if (fn->retty >= 0) {
		typclass(&typ[fn->retty], &cls, &is_ptr);
		fprintf(f, " (result %s)", isint(cls) ? "i32" : "f32");
	}
	for (b=fn->start, i=b->ins; i<&b->ins[b->nins]; i++) {
		if (!ispar(i->op))
			break;
		typclass(&typ[i->arg[0].val], &cls, &is_ptr);
		fprintf(f, " (param $%d %s)", i->to.val, isint(cls) ? "i32" : "f32");
	}
	fprintf(f, "\n");

	for (j=0; j<fn->ntmp; j++)
		if (fn->tmp[j].slot != -1)
			fprintf(f, "    (local $%d %s)\n", j, isint(fn->tmp[j].cls) ? "i32" : "f32");

	for (b=fn->start; b; b=b->link) {
		fprintf(f, "    (block $%s\n", b->name);
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			emit_instr(i, fn, f);
		switch (b->jmp.type) {
		case Jretw:
			if (rtype(b->jmp.arg) == RCon)
				fprintf(f, "    i32.const %"PRId64"\n", fn->con[b->jmp.arg.val].bits.i);
			else
				fprintf(f, "    local.get %d\n", b->jmp.arg.val);
			fprintf(f, "    return\n");
			break;
		case Jret0:
			fprintf(f, "    return\n");
			break;
		case Jjmp:
			fprintf(f, "    br $%s\n", b->s1->name);
			break;
		case Jjnz:
			fprintf(f, "    br_if $%s\n", b->s1->name);
			fprintf(f, "    br $%s\n", b->s2->name);
			break;
		default:
			break;
		}
		fprintf(f, "    )\n");
	}

	fprintf(f, "  )\n");
}

void
wasm_emitfin(FILE *f)
{
	emit_blit(f);
	fprintf(f, ")\n");
	fprintf(f,
		"<!--\n"
		"Minimal HTML+JS harness:\n"
		"<script>\n"
		"  (async () => {\n"
		"    const r = await fetch('qbe.wasm');\n"
		"    const b = await r.arrayBuffer();\n"
		"    const m = await WebAssembly.instantiate(b, {});\n"
		"    console.log(m.instance.exports.main());\n"
		"  })();\n"
		"</script>\n"
		"-->\n"
	);
}