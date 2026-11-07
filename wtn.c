/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#define IOSZ 65536
#define LNSZ 4096
#define MXMAP 4096
#define MXPID 4096
#define PMAX 4096
#define SHL 65
#define DS " (deleted)"
#define SCAP (64ULL << 20)
#define OVL 32
#define BA iob
#define BB (iob + IOSZ)

typedef unsigned long long ull;

struct sh {
	uint32_t h[8];
	uint64_t n;
	unsigned char b[64];
	unsigned l;
};

struct mp {
	ull s, e, o;
	char pr[5];
	char p[PMAX];
};

struct mr {
	ull d, f;
	char hm[SHL], hf[SHL];
};

struct sd {
	int fd;
	const char *lb;
	ull ad, of, n;
};

static const char *const lv[3] = { "LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT" };
static char iob[2 * IOSZ];
static char lb[LNSZ];
static struct mp mt[MXMAP];
static struct mp stk, hp;
static char pb[PMAX + 64];
static char rl[PMAX];
static char ex[PMAX];
static char cm[64];
static char he[SHL];
static int pl[MXPID];
static int np;
static int o_a, o_x, o_v;
static int n_bad, n_unv;
static int cp, gn;
static int rlok, ef, hok, isdel, ismf;
static int nm, nmap, nchk, nst, mok, mu, mf, hstk, hhp, sok;
static ull esz;
static ull sa[4];
static long long tty;

static const uint32_t K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
	0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
	0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
	0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t ror(uint32_t x, int n)
{
	return (x >> n) | (x << (32 - n));
}

static void sh_i(struct sh *s)
{
	static const uint32_t iv[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};

	memcpy(s->h, iv, sizeof iv);
	s->n = 0;
	s->l = 0;
}

static void sh_w(const unsigned char *p, uint32_t *w)
{
	int i;
	for (i = 0; i < 16; i++, p += 4)
		w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
			(uint32_t)p[2] << 8 | p[3];
	for (i = 16; i < 64; i++) {
		uint32_t a = w[i - 15], b = w[i - 2];
		uint32_t s0 = ror(a, 7) ^ ror(a, 18) ^ (a >> 3);
		uint32_t s1 = ror(b, 17) ^ ror(b, 19) ^ (b >> 10);

		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
}

static void sh_t(struct sh *s, const unsigned char *p)
{
	uint32_t w[64], v[8];
	int i, j;
	sh_w(p, w);
	memcpy(v, s->h, sizeof v);
	for (i = 0; i < 64; i++) {
		uint32_t s1 = ror(v[4], 6) ^ ror(v[4], 11) ^ ror(v[4], 25);
		uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
		uint32_t t1 = v[7] + s1 + ch + K[i] + w[i];
		uint32_t s0 = ror(v[0], 2) ^ ror(v[0], 13) ^ ror(v[0], 22);
		uint32_t mj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);

		for (j = 7; j > 0; j--) v[j] = v[j - 1];
		v[4] += t1;
		v[0] = t1 + s0 + mj;
	}
	for (i = 0; i < 8; i++) s->h[i] += v[i];
}

static void sh_u(struct sh *s, const void *d, size_t n)
{
	const unsigned char *p = d;
	s->n += n;
	while (n) {
		size_t c = 64 - s->l;

		if (c > n) c = n;
		memcpy(s->b + s->l, p, c);
		s->l += c;
		p += c;
		n -= c;
		if (s->l == 64) {
			sh_t(s, s->b);
			s->l = 0;
		}
	}
}

static void sh_f(struct sh *s, unsigned char *o)
{
	uint64_t bits = s->n * 8;
	unsigned char c = 0x80, t[8];
	int i;
	sh_u(s, &c, 1);
	c = 0;
	while (s->l != 56) sh_u(s, &c, 1);
	for (i = 0; i < 8; i++) t[i] = bits >> (56 - 8 * i);
	sh_u(s, t, 8);
	for (i = 0; i < 32; i++) o[i] = s->h[i / 4] >> (24 - 8 * (i % 4));
}

static void sh_x(const unsigned char *d, char *o)
{
	static const char hx[] = "0123456789abcdef";
	int i;
	for (i = 0; i < 32; i++) {
		o[2 * i] = hx[d[i] >> 4];
		o[2 * i + 1] = hx[d[i] & 15];
	}
	o[64] = 0;
}

static void sh_e(struct sh *s, char *o)
{
	unsigned char d[32];
	sh_f(s, d);
	sh_x(d, o);
}

static int st_1(const void *d, size_t n, const char *w)
{
	struct sh s;
	char o[SHL];
	sh_i(&s);
	sh_u(&s, d, n);
	sh_e(&s, o);
	return !strcmp(o, w);
}

static int st_m(void)
{
	struct sh s;
	char o[SHL];
	int i;
	memset(BA, 'a', 1000);
	sh_i(&s);
	for (i = 0; i < 1000; i++) sh_u(&s, BA, 1000);
	sh_e(&s, o);
	return !strcmp(o, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

static int st(void)
{
	int ok = st_1("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	ok &= st_1("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	ok &= st_m();
	puts(ok ? "ok" : "FAIL");
	return ok ? 0 : 2;
}

static void pv(const char *s)
{
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;

		if (c <= ' ' || c == 0x7f || c == '\\')
			printf("\\%03o", c);
		else
			putchar(c);
	}
}

static void fv(char c, va_list *a)
{
	ull x;
	if (c == 's') {
		pv(va_arg(*a, const char *));
	} else if (c == 'u') {
		printf("%llu", va_arg(*a, ull));
	} else if (c == 'd') {
		printf("%lld", va_arg(*a, long long));
	} else if (c == 'x') {
		printf("0x%llx", va_arg(*a, ull));
	} else if (c == 'z') {
		printf("0x%016llx", va_arg(*a, ull));
	} else {
		x = va_arg(*a, ull);
		printf("0x%llx-0x%llx", x, va_arg(*a, ull));
	}
}

static void fl(char lv, const char *k, const char *f, ...)
{
	va_list a;
	if (lv == '.' && !o_v) return;
	n_bad += lv == '!' || lv == '?';
	n_unv += lv == 'u';
	if (cp)
		printf("%d %c %s", cp, lv, k);
	else
		printf("- %c %s", lv, k);
	if (*f) putchar(' ');
	va_start(a, f);
	for (; *f; f++) {
		if (*f == '%')
			fv(*++f, &a);
		else
			putchar(*f);
	}
	va_end(a);
	putchar('\n');
}

static void gone(void)
{
	fl('.', "GONE", "");
	gn = 1;
}

static void unv(const char *w)
{
	fl('u', "UNVERIFIED", "which=%s", w);
}

static void fail(const char *w)
{
	if (errno == ENOENT || errno == ESRCH)
		gone();
	else
		unv(w);
}

static void cl(int f)
{
	if (f >= 0 && close(f) < 0) perror("wtn: close");
}

static const char *pp(const char *n)
{
	snprintf(pb, sizeof pb, "/proc/%d/%s", cp, n);
	return pb;
}

static int alive(void)
{
	return access(pp("stat"), F_OK) == 0;
}

static int rdp(const char *n, char *b, size_t cap)
{
	int f = open(pp(n), O_RDONLY | O_CLOEXEC);
	size_t l = 0;
	ssize_t r = 1;
	if (f < 0) {
		fail(n);
		return -1;
	}
	while (l < cap - 1 && r > 0) {
		r = read(f, b + l, cap - 1 - l);
		if (r > 0) l += (size_t)r;
	}
	if (r < 0) fail(n);
	cl(f);
	b[l] = 0;
	return r < 0 ? -1 : (int)l;
}

static ssize_t rf(int f, char *b, size_t n, ull o)
{
	size_t l = 0;
	while (l < n) {
		ssize_t r = pread(f, b + l, n - l, (off_t)(o + l));

		if (r < 0) return -1;
		if (r == 0) break;
		l += (size_t)r;
	}
	return (ssize_t)l;
}

static int hf(int f, char *h, ull *z)
{
	struct sh s;
	ull o = 0;
	ssize_t r;
	sh_i(&s);
	while ((r = rf(f, BA, IOSZ, o)) > 0) {
		sh_u(&s, BA, (size_t)r);
		o += (ull)r;
	}
	if (r < 0) return -1;
	sh_e(&s, h);
	*z = o;
	return 0;
}

static int oq(const char *p)
{
	struct stat b;
	int f = open(p, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (f < 0) return -1;
	if (fstat(f, &b) < 0) b.st_mode = 0;
	if (!S_ISREG(b.st_mode)) {
		cl(f);
		errno = EINVAL;
		return -1;
	}
	return f;
}

static int orp(const char *p)
{
	int r = snprintf(pb, sizeof pb, "/proc/%d/root%s", cp, p);

	if (r < 0 || (size_t)r >= sizeof pb) {
		errno = ENAMETOOLONG;
		return -1;
	}
	return oq(pb);
}

static int ew(const char *s, const char *e)
{
	size_t a = strlen(s), b = strlen(e);
	return a >= b && !strcmp(s + a - b, e);
}

static const char *bn(const char *s)
{
	const char *p = strrchr(s, '/');
	return p ? p + 1 : s;
}

static int t15(const char *x)
{
	return !strncmp(cm, x, 15);
}

static char *sks(char *q)
{
	while (*q && *q != ' ') q++;
	while (*q == ' ') q++;
	return q;
}

static int ld(const struct sd *s, ull r, unsigned char *b)
{
	size_t c = 16;
	ssize_t g;
	if (r >= s->n) return 0;
	if (s->n - r < c) c = (size_t)(s->n - r);
	g = rf(s->fd, (char *)b, c, s->of + r);
	return g < 0 ? 0 : (int)g;
}

static void xrow(const struct sd *s, ull r)
{
	unsigned char b[16];
	int k = ld(s, r, b), i;
	printf("  %s %016llx: ", s->lb, s->ad + r);
	for (i = 0; i < 16; i++) {
		if (i < k)
			printf("%02x", b[i]);
		else
			fputs("..", stdout);
		fputs(i == 7 || i == 15 ? "  " : i < 15 ? " " : "", stdout);
	}
	putchar('|');
	for (i = 0; i < 16; i++)
		putchar(i < k && b[i] >= 0x20 && b[i] < 0x7f ? b[i] : '.');
	puts("|");
}

static void xdf(const struct sd *a, const struct sd *b, ull r)
{
	unsigned char x[16], y[16];
	int ka = ld(a, r, x), kb = ld(b, r, y), i, col = 6;
	fputs("  diff", stdout);
	for (i = 0; i < 16; i++) {
		int at = 25 + 3 * i + (i >= 8);

		if ((i < ka) == (i < kb) && (i >= ka || x[i] == y[i])) continue;
		printf("%*s^^", at - col, "");
		col = at + 2;
	}
	putchar('\n');
}

static void xs(const struct sd *s, const struct sd *o, ull r, ull m)
{
	if (r) xrow(s, r - 16);
	xrow(s, r);
	if (o) xdf(o, s, r);
	if (r + 16 < m) xrow(s, r + 16);
}

static void xd(const struct sd *a, const struct sd *b, ull d)
{
	ull r = d & ~15ULL, m = a->n > b->n ? a->n : b->n;
	xs(a, NULL, r, m);
	xs(b, a, r, m);
}

static ull fdf(int a, int b, ull n)
{
	ull o = 0;
	while (o < n) {
		size_t c = n - o < IOSZ ? (size_t)(n - o) : IOSZ, i;
		ssize_t x = rf(a, BA, c, o), y = rf(b, BB, c, o);

		if (x < 0 || y < 0) return o;
		c = (size_t)(x < y ? x : y);
		for (i = 0; i < c; i++)
			if (BA[i] != BB[i]) return o + i;
		if (!c) return o;
		o += c;
	}
	return n;
}

static void xp(int f, const char *l, ull z)
{
	struct sd a = { ef, "exe ", 0, 0, esz };
	struct sd b = { f, l, 0, 0, z };
	xd(&a, &b, fdf(ef, f, esz < z ? esz : z));
}

static int s0(void)
{
	if (rdp("status", BA, IOSZ) < 0) return -1;
	if (!strstr(BA, "\nVmSize:")) {
		fl('.', "KTHREAD", "");
		return -1;
	}
	return 0;
}

static void s1c(const char *a)
{
	const char *e = bn(ex);
	if (cm[0] && rlok && !t15(e)) {
		fl('?', "ID_COMM_EXE", "comm=%s exe=%s", cm, ex);
	}
	if (!a[0]) return;
	if (cm[0] && !t15(bn(a))) {
		fl('?', "ID_COMM_ARGV", "comm=%s argv0=%s", cm, a);
	}
	if (rlok && strcmp(bn(a), e)) {
		fl('?', "ID_ARGV_EXE", "argv0=%s exe=%s", a, ex);
	}
}

static void s1l(void)
{
	ssize_t r = readlink(pp("exe"), rl, sizeof rl - 1);
	rlok = r >= 0;
	rl[rlok ? r : 0] = 0;
	strcpy(ex, rl);
	if (!rlok) {
		fail("exe_link");
		return;
	}
	if (ew(ex, DS)) ex[strlen(ex) - strlen(DS)] = 0;
}

static void s1(void)
{
	int n = rdp("comm", cm, sizeof cm);
	if (gn) return;
	if (n > 0 && cm[n - 1] == '\n') cm[n - 1] = 0;
	s1l();
	if (gn) return;
	BA[0] = 0;
	if (rdp("cmdline", BA, IOSZ) < 0 && gn) return;
	s1c(BA);
	fl('.', "EXE", "path=%s comm=%s argv0=%s", rl, cm, BA);
}

static void hline(const char *w, const char *h, ull z)
{
	fl('.', "HASH", "witness=%s sha=%s size=%u", w, h, z);
}

static void s2(void)
{
	isdel = rlok && ew(rl, DS);
	ismf = rlok && !strncmp(rl, "/memfd:", 7);
	if (isdel) {
		fl('?', "EXE_DELETED", "path=%s", rl);
	}
	if (ismf) {
		fl('?', "EXE_MEMFD", "path=%s", rl);
	}
	ef = open(pp("exe"), O_RDONLY | O_CLOEXEC);
	if (ef < 0) {
		fail("exe");
		return;
	}
	if (hf(ef, he, &esz) < 0) {
		fail("exe");
		return;
	}
	hok = 1;
	hline("exe", he, esz);
}

static void s3m(int f, const char *h, ull z)
{
	fl('!', "PATH_ROOT_MISMATCH", "path=%s exe_sha=%s root_sha=%s exe_size=%u root_size=%u", ex, he, h, esz, z);
	if (o_x) xp(f, "root", z);
}

static void s3r(void)
{
	char h[SHL];
	ull z;
	int f = orp(ex);

	if (f < 0) {
		if (errno != ENOENT && errno != ENOTDIR) {
			fail("root");
		} else if (!alive()) {
			gone();
		} else {
			fl('?', "PATH_ROOT_MISSING", "path=%s", ex);
		}
		return;
	}
	if (hf(f, h, &z) < 0) {
		fail("root");
	} else {
		hline("root", h, z);
		if (strcmp(h, he)) s3m(f, h, z);
	}
	cl(f);
}

static void s3h(void)
{
	char h[SHL];
	ull z;
	int f = oq(ex);
	if (f < 0) {
		if (errno == ENOENT || errno == ENOTDIR) {
			fl('.', "PATH_HOST_MISSING", "path=%s", ex);
		} else {
			unv("host");
		}
		return;
	}
	if (hf(f, h, &z) < 0) {
		unv("host");
	} else {
		hline("host", h, z);
		if (strcmp(h, he)) {
			fl('?', "PATH_HOST_MISMATCH", "path=%s exe_sha=%s host_sha=%s", ex, he, h);
			if (o_x) xp(f, "host", z);
		}
	}
	cl(f);
}

static void s3(void)
{
	if (!rlok || !hok) return;
	if (isdel || ismf) {
		fl('.', "PATH_GONE", "path=%s", rl);
		return;
	}
	s3r();
	if (!gn) s3h();
}

static int nl1(char *l, struct mp *r)
{
	char *q;
	char *w;
	r->s = strtoull(l, &q, 16);
	if (*q != '-') return -1;
	r->e = strtoull(q + 1, &q, 16);
	if (*q != ' ' || strlen(q) < 6) return -1;
	memcpy(r->pr, q + 1, 4);
	r->pr[4] = 0;
	r->o = strtoull(q + 6, &q, 16);
	q = sks(sks(sks(q)));
	while (*q == ' ') q++;
	for (w = r->p; *q && w < r->p + PMAX - 1; q++) {
		if (!strncmp(q, "\\012", 4)) {
			*w++ = '\n';
			q += 3;
		} else {
			*w++ = *q;
		}
	}
	*w = 0;
	return 0;
}

static void addm(char *l)
{
	static struct mp t;
	if (nl1(l, &t) < 0) {
		unv("maps");
		return;
	}
	nmap++;
	if (!strcmp(t.p, "[stack]")) {
		stk = t;
		hstk = 1;
	}
	if (!strcmp(t.p, "[heap]")) {
		hp = t;
		hhp = 1;
	}
	if (nm < MXMAP)
		mt[nm++] = t;
	else if (nmap == MXMAP + 1)
		unv("maps_cap");
}

static void s4b(const char *b, ssize_t r, size_t *n, int *lg)
{
	ssize_t i;
	for (i = 0; i < r; i++) {
		if (b[i] == '\n') {
			lb[*n] = 0;
			addm(lb);
			*n = 0;
		} else if (*n < LNSZ - 1) {
			lb[(*n)++] = b[i];
		} else if (!*lg) {
			*lg = 1;
			unv("maps_line");
		}
	}
}

static void s4r(void)
{
	int f = open(pp("maps"), O_RDONLY | O_CLOEXEC), lg = 0;
	size_t n = 0;
	ssize_t r;
	if (f < 0) {
		fail("maps");
		return;
	}
	while ((r = read(f, BA, IOSZ)) > 0) s4b(BA, r, &n, &lg);
	if (r < 0)
		fail("maps");
	else
		mok = 1;
	cl(f);
}

static void unm(const struct mp *m)
{
	fl('u', "UNVERIFIED", "which=map_file map=%r", m->s, m->e);
}

static void memu(void)
{
	if (errno == ESRCH || !alive()) {
		gone();
	} else if (!mu) {
		mu = 1;
		unv("mem");
	}
}

static int ismain(const struct mp *m)
{
	return rlok && !strcmp(m->p, rl);
}

static int ob(const struct mp *m, const char **via)
{
	int f;
	if (ismain(m)) {
		*via = "exe";
		return ef < 0 ? -2 : ef;
	}
	*via = "map_files";
	snprintf(pb, sizeof pb, "/proc/%d/map_files/%llx-%llx", cp, m->s, m->e);
	f = oq(pb);
	if (f >= 0 || ew(m->p, DS)) return f;
	*via = "root_path";
	return orp(m->p);
}

static void mdiff(const char *a, const char *b, size_t c, ull o, struct mr *q)
{
	size_t i;
	for (i = 0; i < c; i++) {
		if (a[i] != b[i]) {
			if (!q->d) q->f = o + i;
			q->d++;
		}
	}
}

static int mcmp(int f, const struct mp *m, ull n, struct mr *q)
{
	struct sh a, b;
	ull o = 0;
	sh_i(&a);
	sh_i(&b);
	q->d = 0;
	while (o < n) {
		size_t c = n - o < IOSZ ? (size_t)(n - o) : IOSZ;

		if (rf(mf, BA, c, m->s + o) != (ssize_t)c) return -1;
		if (rf(f, BB, c, m->o + o) != (ssize_t)c) return -2;
		mdiff(BA, BB, c, o, q);
		sh_u(&a, BA, c);
		sh_u(&b, BB, c);
		o += c;
	}
	sh_e(&a, q->hm);
	sh_e(&b, q->hf);
	return 0;
}

static void mline(int f, const struct mp *m, const char *via, ull n, const struct mr *q)
{
	struct sd a = { mf, "mem ", m->s, m->s, n };
	struct sd b = { f, "file", m->o, m->o, n };
	fl('!', "MEM_MISMATCH", "map=%r off=%x path=%s via=%s diff_bytes=%u first=%x mem_sha=%s file_sha=%s", m->s, m->e, m->o, m->p, via, q->d, m->s + q->f, q->hm, q->hf);
	if (o_x) xd(&a, &b, q->f);
}

static void s4f(int f, const struct mp *m, const char *via)
{
	static struct mr q;
	struct stat b;
	ull n = m->e - m->s;
	int r;
	if (fstat(f, &b) < 0) {
		unm(m);
		return;
	}
	if ((ull)b.st_size <= m->o) return;
	if ((ull)b.st_size - m->o < n) n = (ull)b.st_size - m->o;
	r = mcmp(f, m, n, &q);
	if (r == -1) {
		memu();
	} else if (r == -2) {
		unm(m);
	} else {
		nchk++;
		if (q.d) mline(f, m, via, n, &q);
	}
}

static int iscmp(const struct mp *m)
{
	if (strncmp(m->pr, "r-x", 3) || m->p[0] != '/') return 0;
	return ismain(m) || strncmp(m->p, "/memfd:", 7);
}

static void s4c(void)
{
	const char *via;
	int i, f;
	mf = open(pp("mem"), O_RDONLY | O_CLOEXEC);
	if (mf < 0) {
		mu = 1;
		fail("mem");
		return;
	}
	for (i = 0; i < nm && !mu && !gn; i++) {
		if (!iscmp(&mt[i])) continue;
		f = ob(&mt[i], &via);
		if (f == -2) continue;
		if (f < 0) {
			if (alive())
				unm(&mt[i]);
			else
				gone();
			continue;
		}
		s4f(f, &mt[i], via);
		if (f != ef) cl(f);
	}
}

static int isx(const struct mp *m)
{
	return m->pr[2] == 'x';
}

static const char *dsh(const char *s)
{
	return s[0] ? s : "-";
}

static void s4a(const struct mp *m)
{
	const char *p = m->p;
	if (!isx(m) || (p[0] && p[0] != '[')) return;
	if (!strcmp(p, "[vdso]") || !strcmp(p, "[vsyscall]") || !strcmp(p, "[vvar]"))
		return;
	fl('?', "ANON_EXEC", "map=%r perms=%s name=%s", m->s, m->e, m->pr, dsh(p));
}

static void s4w(const struct mp *m)
{
	if (strncmp(m->pr, "rwx", 3)) return;
	fl('?', "WX_MAP", "map=%r perms=%s path=%s", m->s, m->e, m->pr, dsh(m->p));
}

static void s4d(const struct mp *m)
{
	if (!isx(m) || m->p[0] != '/' || ismain(m) || !ew(m->p, DS)) return;
	fl('?', "LIB_DELETED", "path=%s", m->p);
}

static void s4o(const struct mp *m)
{
	static const char *const ok[] = {
		"/lib/", "/lib64/", "/usr/lib/", "/usr/lib64/",
		"/usr/local/lib/", "/usr/libexec/"
	};
	size_t i;
	if (!isx(m) || m->p[0] != '/' || ismain(m) || !strncmp(m->p, "/memfd:", 7))
		return;
	for (i = 0; i < sizeof ok / sizeof ok[0]; i++)
		if (!strncmp(m->p, ok[i], strlen(ok[i]))) return;
	fl('?', "LIB_ODD_PATH", "path=%s", m->p);
}

static void s4(void)
{
	int i;
	s4r();
	if (gn || !mok) return;
	s4c();
	if (gn) return;
	for (i = 0; i < nm; i++) s4a(&mt[i]);
	for (i = 0; i < nm; i++) s4w(&mt[i]);
	for (i = 0; i < nm; i++) s4d(&mt[i]);
	for (i = 0; i < nm; i++) s4o(&mt[i]);
	fl('.', "MAPS", "count=%u exec_checked=%u", (ull)nmap, (ull)nchk);
}

static void s5p(void)
{
	char *q;
	int i;
	if (rdp("stat", BA, IOSZ) < 0) return;
	q = strrchr(BA, ')');
	if (!q) {
		unv("stat");
		return;
	}
	q += 2;
	for (i = 3; i <= 51; i++) {
		if (!*q) {
			unv("stat");
			return;
		}
		if (i == 7) tty = strtoll(q, NULL, 10);
		if (i >= 48) sa[i - 48] = strtoull(q, NULL, 10);
		q = sks(q);
	}
	sok = 1;
}

static void s5a(void)
{
	if (!mok) return;
	if (!hstk) {
		unv("stack");
		return;
	}
	if (sa[2] < stk.s || sa[3] > stk.e) {
		fl('!', "ENV_RELOCATED", "env=%r stack=%r", sa[2], sa[3], stk.s, stk.e);
	}
	if (sa[0] < stk.s || sa[1] > stk.e) {
		fl('?', "ARGV_RELOCATED", "arg=%r stack=%r", sa[0], sa[1], stk.s, stk.e);
	}
}

static int lvm(const char *s, size_t l)
{
	int i;
	for (i = 0; i < 3; i++) {
		size_t n = strlen(lv[i]);

		if (l > n && !memcmp(s, lv[i], n) && s[n] == '=') return i;
	}
	return -1;
}

static void s5e(void)
{
	int n = rdp("environ", iob, sizeof iob);
	char *p = iob;
	while (n > 0 && p < iob + n) {
		size_t l = strlen(p);
		int i = lvm(p, l);

		if (i >= 0) {
			fl('?', "ENV_LOADER_VAR", "name=%s value=%s", lv[i], p + strlen(lv[i]) + 1);
		}
		p += l + 1;
	}
}

static void s5s(const char *b, size_t c, size_t u, ull a)
{
	size_t i;
	for (i = 0; i < u && nst < 8; i++) {
		int k = b[i] == 'L' ? lvm(b + i, c - i) : -1;
		ull d = a + i;

		if (k < 0 || (d >= sa[2] && d < sa[3])) continue;
		nst++;
		fl('?', "ENV_STRAY_LOADER_VAR", "name=%s addr=%x", lv[k], d);
	}
}

static void s5m(const struct mp *m)
{
	ull a = m->s, e = m->e;
	if (e - a > SCAP) e = a + SCAP;
	while (a < e && nst < 8) {
		size_t c = e - a < IOSZ ? (size_t)(e - a) : IOSZ;
		size_t u = a + c < e ? c - OVL : c;

		if (rf(mf, BA, c, a) != (ssize_t)c) {
			memu();
			return;
		}
		s5s(BA, c, u, a);
		a += u;
	}
}

static void s5(void)
{
	s5p();
	if (gn) return;
	if (sok && !sa[0] && !sa[1] && !sa[2] && !sa[3]) {
		sok = 0;
		unv("stat_mm");
	} else if (sok) {
		s5a();
	}
	s5e();
	if (gn || mf < 0 || mu || !sok) return;
	if (hstk) s5m(&stk);
	if (!gn && hhp) s5m(&hp);
}

static char *gl(const char *k)
{
	char *q = strstr(BA, k);
	return q ? q + strlen(k) : NULL;
}

static int fcap(void)
{
	ssize_t r = ef < 0 ? -1 : fgetxattr(ef, "security.capability", NULL, 0);
	if (r > 0) return 1;
	if (r < 0 && (ef < 0 || (errno != ENODATA && errno != ENOTSUP)))
		unv("filecaps");
	return 0;
}

static void s6c(ull cap, ull eu)
{
	int fc;
	if (!eu || !cap) return;
	fc = fcap();
	fl('?', "CAPS_UNPRIVILEGED", "capeff=%z euid=%u filecaps=%s", cap, eu, fc ? "yes" : "no");
}

static int nsj(const char *q)
{
	int n = 1;
	size_t i = 0;
	for (; *q && *q != '\n' && i < LNSZ - 1; q++) {
		if (*q == '\t') n++;
		lb[i++] = *q == '\t' ? '/' : *q;
	}
	lb[i] = 0;
	return n;
}

static void s6l(void)
{
	char *q, *e;
	if (rdp("cgroup", BB, IOSZ) < 0) return;
	e = strchr(BB, '\n');
	if (e) *e = 0;
	q = strrchr(BB, ':');
	fl('.', "LINEAGE", "ppid=1 cgroup=%s tty=%d", q ? q + 1 : BB, tty);
}

static void s6(void)
{
	char *u, *c, *n, *p;
	ull ru = 0, eu = 0;
	if (rdp("status", BA, IOSZ) < 0) return;
	u = gl("\nUid:\t");
	c = gl("\nCapEff:\t");
	n = gl("\nNSpid:\t");
	p = gl("\nPPid:\t");
	if (u) {
		ru = strtoull(u, &u, 10);
		eu = strtoull(u, NULL, 10);
	}
	if (u && c) s6c(strtoull(c, NULL, 16), eu);
	if (u && ru != eu) {
		fl('.', "IDS_SPLIT", "ruid=%u euid=%u", ru, eu);
	}
	if (n && nsj(n) > 1) {
		fl('.', "NS_NESTED", "nspid=%s", lb);
	}
	if (p && strtoull(p, NULL, 10) == 1 && sok) s6l();
}

static void dop(int p)
{
	cp = p;
	gn = rlok = hok = isdel = ismf = 0;
	nm = nmap = nchk = nst = mok = mu = hstk = hhp = sok = 0;
	ef = mf = -1;
	cm[0] = 0;
	if (s0() < 0) return;
	s1();
	if (!gn) s2();
	if (!gn) s3();
	if (!gn) s4();
	if (!gn) s5();
	if (!gn) s6();
	cl(mf);
	cl(ef);
}

static void use(void)
{
	fputs("usage: wtn [-a] [-x] [-v] [-T] [pid ...]\n", stderr);
	exit(2);
}

static int pi(const char *s)
{
	long v = 0;

	if (!*s) return 0;
	for (; *s; s++) {
		if (*s < '0' || *s > '9') return 0;
		v = v * 10 + (*s - '0');
		if (v > INT_MAX) return 0;
	}
	return (int)v;
}

static int seen(int p)
{
	int i;
	for (i = 0; i < np; i++)
		if (pl[i] == p) return 1;
	return 0;
}

static void ldso(void)
{
	struct stat b;
	cp = 0;
	if (stat("/etc/ld.so.preload", &b) < 0) {
		if (errno != ENOENT) unv("ld_so_preload");
		return;
	}
	if (b.st_size > 0) {
		fl('?', "LDSO_PRELOAD", "path=/etc/ld.so.preload");
	}
}

static void scan(void)
{
	struct dirent *e;
	DIR *d = opendir("/proc");
	int me = getpid(), p;
	if (!d) {
		perror("wtn: /proc");
		exit(2);
	}
	while ((e = readdir(d))) {
		p = pi(e->d_name);
		if (p && p != me && !seen(p)) dop(p);
	}
	if (closedir(d) < 0) perror("wtn: closedir");
	ldso();
}

static void args(int ac, char **av)
{
	int i, p;
	for (i = optind; i < ac; i++) {
		p = pi(av[i]);
		if (!p) use();
		if (seen(p)) continue;
		if (np == MXPID) use();
		pl[np++] = p;
	}
}

int main(int ac, char **av)
{
	int c, t = 0, i;
	while ((c = getopt(ac, av, "axvT")) != -1) {
		if (c == 'a')
			o_a = 1;
		else if (c == 'x')
			o_x = 1;
		else if (c == 'v')
			o_v = 1;
		else if (c == 'T')
			t = 1;
		else
			use();
	}
	if (t) return st();
	args(ac, av);
	if (!np && !o_a) use();
	for (i = 0; i < np; i++) dop(pl[i]);
	if (o_a) scan();
	if (fflush(stdout) != 0) return 2;
	return n_bad ? 1 : n_unv ? 3 : 0;
}
