/*
 * FILE:	dtrace_probe_ctx.c
 * DESCRIPTION:	Dynamic Tracing: probe context functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include "dtrace.h"

static void dtrace_panic(const char *fmt, ...)
{
	va_list		alist;

	va_start(alist, fmt);
	vprintk(fmt, alist);
	va_end(alist);

	BUG();
}

int dtrace_assfail(const char *a, const char *f, int l)
{
	dtrace_panic("assertion failed: %s, file: %s, line: %d", a, f, l);

	/*
	 * FIXME: We can do better than this.  The OpenSolaris DTrace source
	 * states that this cannot be optimized away.
	 */
	return a[(uintptr_t)f];
}
EXPORT_SYMBOL(dtrace_assfail);

#define DT_MASK_LO	0x00000000FFFFFFFFULL

static void dtrace_add_128(uint64_t *addend1, uint64_t *addend2, uint64_t *sum)
{
	uint64_t	result[2];

	result[0] = addend1[0] + addend2[0];
	result[1] = addend1[1] + addend2[1] +
		    (result[0] < addend1[0] || result[0] < addend2[0] ? 1 : 0);

	sum[0] = result[0];
	sum[1] = result[1];
}

static void dtrace_shift_128(uint64_t *a, int b)
{
	uint64_t	mask;

	if (b == 0)
		return;

	if (b < 0) {
		b = -b;

		if (b >= 64) {
			a[0] = a[1] >> (b - 64);
			a[1] = 0;
		} else {
			a[0] >>= b;
			mask = 1LL << (64 - b);
			mask -= 1;
			a[0] |= ((a[1] & mask) << (64 - b));
			a[1] >>= b;
		}
	} else {
		if (b >= 64) {
			a[1] = a[0] << (b - 64);
			a[0] = 0;
		} else {
			a[1] <<= b;
			mask = a[0] >> (64 - b);
			a[1] |= mask;
			a[0] <<= b;
		}
	}
}

static void dtrace_multiply_128(uint64_t factor1, uint64_t factor2,
				uint64_t *product)
{
	uint64_t	hi1, hi2, lo1, lo2;
	uint64_t	tmp[2];

	hi1 = factor1 >> 32;
	hi2 = factor2 >> 32;

	lo1 = factor1 & DT_MASK_LO;
	lo2 = factor2 & DT_MASK_LO;

	product[0] = lo1 * lo2;
	product[1] = hi1 * hi2;

	tmp[0] = hi1 * lo2;
	tmp[1] = 0;
	dtrace_shift_128(tmp, 32);
	dtrace_add_128(product, tmp, product);

	tmp[0] = hi2 * lo1;
	tmp[1] = 0;
	dtrace_shift_128(tmp, 32);
	dtrace_add_128(product, tmp, product);
}

void dtrace_aggregate_min(uint64_t *oval, uint64_t nval, uint64_t arg)
{
	if ((int64_t)nval < (int64_t)*oval)
		*oval = nval;
}

void dtrace_aggregate_max(uint64_t *oval, uint64_t nval, uint64_t arg)
{
	if ((int64_t)nval > (int64_t)*oval)
		*oval = nval;
}

void dtrace_aggregate_quantize(uint64_t *quanta, uint64_t nval, uint64_t incr)
{
	int	i, zero = DTRACE_QUANTIZE_ZEROBUCKET;
	int64_t	val = (int64_t)nval;

	if (val < 0) {
		for (i = 0; i < zero; i++) {
			if (val <= DTRACE_QUANTIZE_BUCKETVAL(i)) {
				quanta[i] += incr;

				return;
			}
		}
	} else {
		for (i = zero + 1; i < DTRACE_QUANTIZE_NBUCKETS; i++) {
			if (val < DTRACE_QUANTIZE_BUCKETVAL(i)) {
				quanta[i - 1] += incr;

				return;
			}
		}

		quanta[DTRACE_QUANTIZE_NBUCKETS - 1] += incr;

		return;
	}

	ASSERT(0);
}

void dtrace_aggregate_lquantize(uint64_t *lquanta, uint64_t nval,
				uint64_t incr)
{
	uint64_t	arg = *lquanta++;
	int32_t		base = DTRACE_LQUANTIZE_BASE(arg);
	uint16_t	step = DTRACE_LQUANTIZE_STEP(arg);
	uint16_t	levels = DTRACE_LQUANTIZE_LEVELS(arg);
	int32_t		val = (int32_t)nval, level;

	ASSERT(step != 0);
	ASSERT(levels != 0);

	if (val < base) {
		lquanta[0] += incr;

		return;
	}

	level = (val - base) / step;

	if (level < levels) {
		lquanta[level + 1] += incr;

		return;
	}

	lquanta[levels + 1] += incr;
}

void dtrace_aggregate_avg(uint64_t *data, uint64_t nval, uint64_t arg)
{
	data[0]++;
	data[1] += nval;
}

void dtrace_aggregate_stddev(uint64_t *data, uint64_t nval, uint64_t arg)
{
	int64_t		snval = (int64_t)nval;
	uint64_t	tmp[2];

	data[0]++;
	data[1] += nval;

	if (snval < 0)
		snval = -snval;

	dtrace_multiply_128((uint64_t)snval, (uint64_t)snval, tmp);
	dtrace_add_128(data + 2, tmp, data + 2);
}

void dtrace_aggregate_count(uint64_t *oval, uint64_t nval, uint64_t arg)
{
	*oval = *oval + 1;
}

void dtrace_aggregate_sum(uint64_t *oval, uint64_t nval, uint64_t arg)
{
	*oval += nval;
}
