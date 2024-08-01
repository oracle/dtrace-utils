/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <alloca.h>
#include <dt_impl.h>
#include <dt_module.h>
#include <dt_pcap.h>
#include <dt_peb.h>
#include <dt_state.h>
#include <dt_string.h>
#include <libproc.h>
#include <port.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <linux/perf_event.h>

#define	DT_MASK_LO 0x00000000FFFFFFFFULL

/*
 * We declare this here because (1) we need it and (2) we want to avoid a
 * dependency on libm in libdtrace.
 */
static long double
dt_fabsl(long double x)
{
	if (x < 0)
		return -x;

	return x;
}

/*
 * 128-bit arithmetic functions needed to support the stddev() aggregating
 * action.
 */
static int
dt_gt_128(uint64_t *a, uint64_t *b)
{
	return a[1] > b[1] || (a[1] == b[1] && a[0] > b[0]);
}

static int
dt_ge_128(uint64_t *a, uint64_t *b)
{
	return a[1] > b[1] || (a[1] == b[1] && a[0] >= b[0]);
}

static int
dt_le_128(uint64_t *a, uint64_t *b)
{
	return a[1] < b[1] || (a[1] == b[1] && a[0] <= b[0]);
}

/*
 * Shift the 128-bit value in a by b. If b is positive, shift left.
 * If b is negative, shift right.
 */
static void
dt_shift_128(uint64_t *a, int b)
{
	uint64_t mask;

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

static int
dt_nbits_128(uint64_t *a)
{
	int nbits = 0;
	uint64_t tmp[2];
	uint64_t zero[2] = { 0, 0 };

	tmp[0] = a[0];
	tmp[1] = a[1];

	dt_shift_128(tmp, -1);
	while (dt_gt_128(tmp, zero)) {
		dt_shift_128(tmp, -1);
		nbits++;
	}

	return nbits;
}

static void
dt_subtract_128(uint64_t *minuend, uint64_t *subtrahend, uint64_t *difference)
{
	uint64_t result[2];

	result[0] = minuend[0] - subtrahend[0];
	result[1] = minuend[1] - subtrahend[1] -
	    (minuend[0] < subtrahend[0] ? 1 : 0);

	difference[0] = result[0];
	difference[1] = result[1];
}

static void
dt_add_128(uint64_t *addend1, uint64_t *addend2, uint64_t *sum)
{
	uint64_t result[2];

	result[0] = addend1[0] + addend2[0];
	result[1] = addend1[1] + addend2[1] +
	    (result[0] < addend1[0] || result[0] < addend2[0] ? 1 : 0);

	sum[0] = result[0];
	sum[1] = result[1];
}

/*
 * The basic idea is to break the 2 64-bit values into 4 32-bit values,
 * use native multiplication on those, and then re-combine into the
 * resulting 128-bit value.
 *
 * (hi1 << 32 + lo1) * (hi2 << 32 + lo2) =
 *     hi1 * hi2 << 64 +
 *     hi1 * lo2 << 32 +
 *     hi2 * lo1 << 32 +
 *     lo1 * lo2
 */
static void
dt_multiply_128(uint64_t factor1, uint64_t factor2, uint64_t *product)
{
	uint64_t hi1, hi2, lo1, lo2;
	uint64_t tmp[2];

	hi1 = factor1 >> 32;
	hi2 = factor2 >> 32;

	lo1 = factor1 & DT_MASK_LO;
	lo2 = factor2 & DT_MASK_LO;

	product[0] = lo1 * lo2;
	product[1] = hi1 * hi2;

	tmp[0] = hi1 * lo2;
	tmp[1] = 0;
	dt_shift_128(tmp, 32);
	dt_add_128(product, tmp, product);

	tmp[0] = hi2 * lo1;
	tmp[1] = 0;
	dt_shift_128(tmp, 32);
	dt_add_128(product, tmp, product);
}

/*
 * This is long-hand division.
 *
 * We initialize subtrahend by shifting divisor left as far as possible. We
 * loop, comparing subtrahend to dividend:  if subtrahend is smaller, we
 * subtract and set the appropriate bit in the result.  We then shift
 * subtrahend right by one bit for the next comparison.
 */
static void
dt_divide_128(uint64_t *dividend, uint64_t divisor, uint64_t *quotient)
{
	uint64_t result[2] = { 0, 0 };
	uint64_t remainder[2];
	uint64_t subtrahend[2];
	uint64_t divisor_128[2];
	uint64_t mask[2] = { 1, 0 };
	int log = 0;

	assert(divisor != 0);

	divisor_128[0] = divisor;
	divisor_128[1] = 0;

	remainder[0] = dividend[0];
	remainder[1] = dividend[1];

	subtrahend[0] = divisor;
	subtrahend[1] = 0;

	while (divisor > 0) {
		log++;
		divisor >>= 1;
	}

	dt_shift_128(subtrahend, 128 - log);
	dt_shift_128(mask, 128 - log);

	while (dt_ge_128(remainder, divisor_128)) {
		if (dt_ge_128(remainder, subtrahend)) {
			dt_subtract_128(remainder, subtrahend, remainder);
			result[0] |= mask[0];
			result[1] |= mask[1];
		}

		dt_shift_128(subtrahend, -1);
		dt_shift_128(mask, -1);
	}

	quotient[0] = result[0];
	quotient[1] = result[1];
}

/*
 * This is the long-hand method of calculating a square root.
 * The algorithm is as follows:
 *
 * 1. Group the digits by 2 from the right.
 * 2. Over the leftmost group, find the largest single-digit number
 *    whose square is less than that group.
 * 3. Subtract the result of the previous step (2 or 4, depending) and
 *    bring down the next two-digit group.
 * 4. For the result R we have so far, find the largest single-digit number
 *    x such that 2 * R * 10 * x + x^2 is less than the result from step 3.
 *    (Note that this is doubling R and performing a decimal left-shift by 1
 *    and searching for the appropriate decimal to fill the one's place.)
 *    The value x is the next digit in the square root.
 * Repeat steps 3 and 4 until the desired precision is reached.  (We're
 * dealing with integers, so the above is sufficient.)
 *
 * In decimal, the square root of 582,734 would be calculated as so:
 *
 *     __7__6__3
 *    | 58 27 34
 *     -49       (7^2 == 49 => 7 is the first digit in the square root)
 *      --
 *       9 27    (Subtract and bring down the next group.)
 * 146   8 76    (2 * 7 * 10 * 6 + 6^2 == 876 => 6 is the next digit in
 *      -----     the square root)
 *         51 34 (Subtract and bring down the next group.)
 * 1523    45 69 (2 * 76 * 10 * 3 + 3^2 == 4569 => 3 is the next digit in
 *         -----  the square root)
 *          5 65 (remainder)
 *
 * The above algorithm applies similarly in binary, but note that the
 * only possible non-zero value for x in step 4 is 1, so step 4 becomes a
 * simple decision: is 2 * R * 2 * 1 + 1^2 (aka R << 2 + 1) less than the
 * preceding difference?
 *
 * In binary, the square root of 11011011 would be calculated as so:
 *
 *     __1__1__1__0
 *    | 11 01 10 11
 *      01          (0 << 2 + 1 == 1 < 11 => this bit is 1)
 *      --
 *      10 01 10 11
 * 101   1 01       (1 << 2 + 1 == 101 < 1001 => next bit is 1)
 *      -----
 *       1 00 10 11
 * 1101    11 01    (11 << 2 + 1 == 1101 < 10010 => next bit is 1)
 *       -------
 *          1 01 11
 * 11101    1 11 01 (111 << 2 + 1 == 11101 > 10111 => last bit is 0)
 *
 */
static uint64_t
dt_sqrt_128(uint64_t *square)
{
	uint64_t result[2] = { 0, 0 };
	uint64_t diff[2] = { 0, 0 };
	uint64_t one[2] = { 1, 0 };
	uint64_t next_pair[2];
	uint64_t next_try[2];
	uint64_t bit_pairs, pair_shift;
	int i;

	bit_pairs = dt_nbits_128(square) / 2;
	pair_shift = bit_pairs * 2;

	for (i = 0; i <= bit_pairs; i++) {
		/*
		 * Bring down the next pair of bits.
		 */
		next_pair[0] = square[0];
		next_pair[1] = square[1];
		dt_shift_128(next_pair, -pair_shift);
		next_pair[0] &= 0x3;
		next_pair[1] = 0;

		dt_shift_128(diff, 2);
		dt_add_128(diff, next_pair, diff);

		/*
		 * next_try = R << 2 + 1
		 */
		next_try[0] = result[0];
		next_try[1] = result[1];
		dt_shift_128(next_try, 2);
		dt_add_128(next_try, one, next_try);

		if (dt_le_128(next_try, diff)) {
			dt_subtract_128(diff, next_try, diff);
			dt_shift_128(result, 1);
			dt_add_128(result, one, result);
		} else {
			dt_shift_128(result, 1);
		}

		pair_shift -= 2;
	}

	assert(result[1] == 0);

	return result[0];
}

uint64_t
dt_stddev(uint64_t *data, uint64_t normal)
{
	uint64_t avg_of_squares[2];
	uint64_t square_of_avg[2];
	int64_t norm_avg;
	uint64_t diff[2];

	/*
	 * The standard approximation for standard deviation is
	 * sqrt(average(x**2) - average(x)**2), i.e. the square root
	 * of the average of the squares minus the square of the average.
	 */
	dt_divide_128(data + 2, normal, avg_of_squares);
	dt_divide_128(avg_of_squares, data[0], avg_of_squares);

	norm_avg = (int64_t)data[1] / (int64_t)normal / (int64_t)data[0];

	if (norm_avg < 0)
		norm_avg = -norm_avg;

	dt_multiply_128((uint64_t)norm_avg, (uint64_t)norm_avg, square_of_avg);

	dt_subtract_128(avg_of_squares, square_of_avg, diff);

	return dt_sqrt_128(diff);
}

static uint32_t
dt_spec_buf_hval(const dt_spec_buf_t *head)
{
	uint32_t g = 0, hval = 0;
	uint32_t *p = (uint32_t *) &head->dtsb_id;

	while ((uintptr_t) p < (((uintptr_t) &head->dtsb_id) +
				sizeof(head->dtsb_id))) {
		hval = (hval << 4) + *p++;
		g = hval & 0xf0000000;
		if (g != 0) {
			hval ^= (g >> 24);
			hval ^= g;
		}
	}

	return hval;
}

static int
dt_spec_buf_cmp(const dt_spec_buf_t *p,
		const dt_spec_buf_t *q)
{
	if (p->dtsb_id == q->dtsb_id)
		return 0;
	return 1;
}

DEFINE_HE_STD_LINK_FUNCS(dt_spec_buf, dt_spec_buf_t, dtsb_he);

static void
dt_spec_buf_destroy(dtrace_hdl_t *dtp, dt_spec_buf_t *dtsb);

static void *
dt_spec_buf_del_buf(dt_spec_buf_t *head, dt_spec_buf_t *ent)
{
	head = dt_spec_buf_del(head, ent);
	dt_spec_buf_destroy(ent->dtsb_dtp, ent);
	return head;
}

static dt_htab_ops_t dt_spec_buf_htab_ops = {
	.hval = (htab_hval_fn)dt_spec_buf_hval,
	.cmp = (htab_cmp_fn)dt_spec_buf_cmp,
	.add = (htab_add_fn)dt_spec_buf_add,
	.del = (htab_del_fn)dt_spec_buf_del_buf,
	.next = (htab_next_fn)dt_spec_buf_next
};

static int
dt_flowindent(dtrace_hdl_t *dtp, dtrace_probedata_t *data, dtrace_epid_t last,
	      dtrace_epid_t next)
{
	dtrace_probedesc_t	*pd = data->dtpda_pdesc, *npd;
	dtrace_datadesc_t	*ndd;
	dtrace_flowkind_t	flow = DTRACEFLOW_NONE;
	const char		*p = pd->prv;
	const char		*n = pd->prb;
	const char		*str = NULL;
	char			*sub;
	static const char	*e_str[2] = { " -> ", " => " };
	static const char	*r_str[2] = { " <- ", " <= " };
	static const char	*ent = "entry", *ret = "return";
	static int		entlen = 0, retlen = 0;
	dtrace_epid_t		id = data->dtpda_epid;
	int			rval;

	if (entlen == 0) {
		assert(retlen == 0);
		entlen = strlen(ent);
		retlen = strlen(ret);
	}

	/*
	 * If the name of the probe is "entry" or ends with "-entry", we
	 * treat it as an entry; if it is "return" or ends with "-return",
	 * we treat it as a return.  (This allows application-provided probes
	 * like "method-entry" or "function-entry" to participate in flow
	 * indentation -- without accidentally misinterpreting popular probe
	 * names like "carpentry", "gentry" or "Coventry".)
	 */
	if ((sub = strstr(n, ent)) != NULL && sub[entlen] == '\0' &&
	    (sub == n || sub[-1] == '-')) {
		flow = DTRACEFLOW_ENTRY;
		str = e_str[strcmp(p, "syscall") == 0];
	} else if ((sub = strstr(n, ret)) != NULL && sub[retlen] == '\0' &&
		   (sub == n || sub[-1] == '-')) {
		flow = DTRACEFLOW_RETURN;
		str = r_str[strcmp(p, "syscall") == 0];
	}

	/*
	 * If we're going to indent this, we need to check the ID of our last
	 * call.  If we're looking at the same probe ID but a different EPID,
	 * we _don't_ want to indent.  (Yes, there are some minor holes in
	 * this scheme -- it's a heuristic.)
	 */
	if (flow == DTRACEFLOW_ENTRY) {
		if (last != DTRACE_EPIDNONE && id != last &&
		    pd->id == dtp->dt_pdesc[last]->id)
			flow = DTRACEFLOW_NONE;
	}

	/*
	 * If we're going to unindent this, it's more difficult to see if
	 * we don't actually want to unindent it -- we need to look at the
	 * _next_ EPID.
	 */
	if (flow == DTRACEFLOW_RETURN && next != DTRACE_EPIDNONE &&
	    next != id) {
		rval = dt_epid_lookup(dtp, next, &ndd, &npd);
		if (rval != 0)
			return rval;

		if (npd->id == pd->id)
			flow = DTRACEFLOW_NONE;
	}

	if (flow == DTRACEFLOW_ENTRY || flow == DTRACEFLOW_RETURN)
		data->dtpda_prefix = str;
	else
		data->dtpda_prefix = "| ";

	if (flow == DTRACEFLOW_RETURN && data->dtpda_indent > 0)
		data->dtpda_indent -= 2;

	data->dtpda_flow = flow;

	return 0;
}

static int
dt_nullprobe()
{
	return DTRACE_CONSUME_THIS;
}

static int
dt_nullrec()
{
	return DTRACE_CONSUME_NEXT;
}

int
dt_read_scalar(caddr_t addr, const dtrace_recdesc_t *rec, uint64_t *valp)
{
	addr += rec->dtrd_offset;

	switch (rec->dtrd_size) {
	case sizeof(uint64_t):
		*valp = *((uint64_t *)addr);
		break;
	case sizeof(uint32_t):
		*valp = *((uint32_t *)addr);
		break;
	case sizeof(uint16_t):
		*valp = *((uint16_t *)addr);
		break;
	case sizeof(uint8_t):
		*valp = *((uint8_t *)addr);
		break;
	default:
		return -1;
	}

	return 0;
}

int
dt_print_quantline(dtrace_hdl_t *dtp, FILE *fp, int64_t val,
    uint64_t normal, long double total, char positives, char negatives)
{
	long double f;
	uint_t depth, len = 40;

	const char *ats = "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@";
	const char *spaces = "                                        ";

	assert(strlen(ats) == len && strlen(spaces) == len);
	assert(!(total == 0 && (positives || negatives)));
	assert(!(val < 0 && !negatives));
	assert(!(val > 0 && !positives));
	assert(!(val != 0 && total == 0));

	if (!negatives) {
		if (positives) {
			f = (dt_fabsl((long double)val) * len) / total;
			depth = (uint_t)(f + 0.5);
		} else {
			depth = 0;
		}

		return dt_printf(dtp, fp, "|%s%s %-9lld\n", ats + len - depth,
		    spaces + depth, (long long)val / normal);
	}

	if (!positives) {
		f = (dt_fabsl((long double)val) * len) / total;
		depth = (uint_t)(f + 0.5);

		return dt_printf(dtp, fp, "%s%s| %-9lld\n", spaces + depth,
		    ats + len - depth, (long long)val / normal);
	}

	/*
	 * If we're here, we have both positive and negative bucket values.
	 * To express this graphically, we're going to generate both positive
	 * and negative bars separated by a centerline.  These bars are half
	 * the size of normal quantize()/lquantize() bars, so we divide the
	 * length in half before calculating the bar length.
	 */
	len /= 2;
	ats = &ats[len];
	spaces = &spaces[len];

	f = (dt_fabsl((long double)val) * len) / total;
	depth = (uint_t)(f + 0.5);

	if (val <= 0)
		return dt_printf(dtp, fp, "%s%s|%*s %-9lld\n", spaces + depth,
		    ats + len - depth, len, "", (long long)val / normal);
	else
		return dt_printf(dtp, fp, "%20s|%s%s %-9lld\n", "",
		    ats + len - depth, spaces + depth,
		    (long long)val / normal);
}

int
dt_print_quantize(dtrace_hdl_t *dtp, FILE *fp, const void *addr,
    size_t size, uint64_t normal)
{
	const int64_t *data = addr;
	int i, first_bin = 0, last_bin = DTRACE_QUANTIZE_NBUCKETS - 1;
	long double total = 0;
	char positives = 0, negatives = 0;

	if (size != DTRACE_QUANTIZE_NBUCKETS * sizeof(uint64_t))
		return dt_set_errno(dtp, EDT_DMISMATCH);

	while (first_bin <= last_bin && data[first_bin] == 0)
		first_bin++;

	if (first_bin > last_bin) {
		/*
		 * There isn't any data.  This is possible if (and only if)
		 * negative increment values have been used.  In this case,
		 * we'll print the buckets around 0.
		 */
		first_bin = DTRACE_QUANTIZE_ZEROBUCKET - 1;
		last_bin = DTRACE_QUANTIZE_ZEROBUCKET + 1;
	} else {
		if (first_bin > 0)
			first_bin--;

		while (last_bin > 0 && data[last_bin] == 0)
			last_bin--;

		if (last_bin < DTRACE_QUANTIZE_NBUCKETS - 1)
			last_bin++;
	}

	for (i = first_bin; i <= last_bin; i++) {
		positives |= (data[i] > 0);
		negatives |= (data[i] < 0);
		total += dt_fabsl((long double)data[i]);
	}

	if (dt_printf(dtp, fp, "\n%16s %41s %-9s\n", "value",
	    "------------- Distribution -------------", "count") < 0)
		return -1;

	for (i = first_bin; i <= last_bin; i++) {
		if (dt_printf(dtp, fp, "%16lld ",
		    (long long)DTRACE_QUANTIZE_BUCKETVAL(i)) < 0)
			return -1;

		if (dt_print_quantline(dtp, fp, data[i], normal, total,
		    positives, negatives) < 0)
			return -1;
	}

	return 0;
}

int
dt_print_lquantize(dtrace_hdl_t *dtp, FILE *fp, const void *addr, size_t size,
		   uint64_t normal, uint64_t sig)
{
	const int64_t *data = addr;
	int i, first_bin = 0, last_bin, base;
	long double total = 0;
	uint16_t step, levels;
	char positives = 0, negatives = 0;

	if (size < sizeof(uint64_t))
		return dt_set_errno(dtp, EDT_DMISMATCH);

	base = DTRACE_LQUANTIZE_BASE(sig);
	step = DTRACE_LQUANTIZE_STEP(sig);
	levels = DTRACE_LQUANTIZE_LEVELS(sig);

	last_bin = levels + 1;

	if (size != sizeof(uint64_t) * (levels + 2))
		return dt_set_errno(dtp, EDT_DMISMATCH);

	while (first_bin <= last_bin && data[first_bin] == 0)
		first_bin++;

	if (first_bin > last_bin) {
		first_bin = 0;
		last_bin = 2;
	} else {
		if (first_bin > 0)
			first_bin--;

		while (last_bin > 0 && data[last_bin] == 0)
			last_bin--;

		if (last_bin < levels + 1)
			last_bin++;
	}

	for (i = first_bin; i <= last_bin; i++) {
		positives |= (data[i] > 0);
		negatives |= (data[i] < 0);
		total += dt_fabsl((long double)data[i]);
	}

	if (dt_printf(dtp, fp, "\n%16s %41s %-9s\n", "value",
	    "------------- Distribution -------------", "count") < 0)
		return -1;

	for (i = first_bin; i <= last_bin; i++) {
		char c[32];
		int err;

		if (i == 0) {
			snprintf(c, sizeof(c), "< %d", base);
			err = dt_printf(dtp, fp, "%16s ", c);
		} else if (i == levels + 1) {
			snprintf(c, sizeof(c), ">= %d", base + (levels * step));
			err = dt_printf(dtp, fp, "%16s ", c);
		} else {
			err = dt_printf(dtp, fp, "%16d ",
			    base + (i - 1) * step);
		}

		if (err < 0 || dt_print_quantline(dtp, fp, data[i], normal,
		    total, positives, negatives) < 0)
			return -1;
	}

	return 0;
}

int
dt_print_llquantize(dtrace_hdl_t *dtp, FILE *fp, const void *addr, size_t size,
		    uint64_t normal, uint64_t sig)
{
	const int64_t *data = addr;
	int factor, lmag, hmag, steps, steps_factor, step, bin0;
	int first_bin = 0, last_bin, nbins, err, i, mag;
	int cwidth = 16, pad = 0;
	char *c;
	uint64_t scale;
	long double total = 0;
	char positives = 0, negatives = 0;

	if (size < sizeof(uint64_t))
		return dt_set_errno(dtp, EDT_DMISMATCH);

	factor = DTRACE_LLQUANTIZE_FACTOR(sig);
	lmag = DTRACE_LLQUANTIZE_LMAG(sig);
	hmag = DTRACE_LLQUANTIZE_HMAG(sig);
	steps = DTRACE_LLQUANTIZE_STEPS(sig);
	steps_factor = steps / factor;
	bin0 = 1 + (hmag-lmag+1) * (steps-steps_factor);

	/*
	 * The rest of the buffer contains:
	 *
	 *   < 0   overflow bin
	 *   < 0   (hmag-lmag+1) log ranges, (steps - steps/factor) bins/range
	 *         underflow bin
	 *   > 0   (hmag-lmag+1) log ranges, (steps - steps/factor) bins/range
	 *   > 0   overflow bin
	 *
	 * Note we could calculate the total nbins from size,
	 * but we do it from first principles so that we can sanity-check size.
	 */
	nbins = (hmag - lmag + 1) * (steps - steps_factor) * 2 + 2 + 1;

	if (size != sizeof(uint64_t) * nbins)
		return dt_set_errno(dtp, EDT_DMISMATCH);

	/* look for first and last bins with data */
	last_bin = nbins - 1;
	while (first_bin <= last_bin && data[first_bin] == 0)
		first_bin++;
	if (first_bin > last_bin) {
		/* report at least one bin so output is not empty */
		first_bin = bin0 + 1;
		last_bin = bin0 + 1;
	} else {
		while (data[last_bin] == 0)
			last_bin--;
	}

	/* see if there are positive or negative counts or both */
	for (i = first_bin; i <= last_bin; i++) {
		positives |= (data[i] > 0);
		negatives |= (data[i] < 0);
		total += dt_fabsl((long double)data[i]);
	}

	if (dt_printf(dtp, fp, "\n%16s %41s %-9s\n", "value",
	    "------------- Distribution -------------", "count") < 0)
		return -1;

	/* broaden the range a little bit for reporting purposes */
	if (first_bin > 0)
		first_bin--;
	if (last_bin < nbins - 1)
		last_bin++;
	/*
	 * There is a possible "ghost bin" problem,
	 * where first_bin or last_bin may end up on a fictitious bin.
	 * Add some padding for when we figure out the labels.
	 */
	if (lmag == 0 && steps > factor) {
		pad = steps_factor;
	}

	/* it is easiest first to populate the labels */
	c = alloca(cwidth * (last_bin-first_bin+1+2*pad));
	memset(c, 0, cwidth * (last_bin-first_bin+1+2*pad));

#define CPRINT(I,FORMAT,VAL) \
	if ((I) >= first_bin - pad && (I) <= last_bin + pad) \
	    snprintf(c + cwidth * ((I) - first_bin + pad), cwidth, FORMAT, VAL)

	scale = (uint64_t)powl(factor, lmag);
	/*
	 * bin0 is for "abs(value)<scale".  Note:
	 *   - "abs() < 1" is better written as "0"
	 *   - first_bin>=bin0 means this and all negative bins have count 0.
	 *     Might as well call this bin "<scale" and not mention "abs()".
	 */
	if (scale == 1) {
		CPRINT(bin0, "%d", (int)0);
	} else if (first_bin >= bin0) {
		CPRINT(bin0, "< %llu", (long long unsigned)scale);
	} else {
		CPRINT(bin0, "abs() < %llu", (long long unsigned)scale);
	}
	CPRINT((bin0-1), "-%llu", (long long unsigned)scale);
	CPRINT((bin0+1), "%llu", (long long unsigned)scale);
	mag = lmag;
	i = 1;
	if (lmag == 0 && steps > factor) {
		for (step = 2; step <= factor; step++) {
			i += steps_factor;
			CPRINT((bin0-i), "-%d", step);
			CPRINT((bin0+i), "%d", step);
		}
		mag++;
	}
	scale = (uint64_t)powl(factor, mag + 1) / steps;
	for ( ; mag <= hmag; mag++) {
		for (step = steps_factor + 1; step <= steps; step++) {
			i++;
			CPRINT((bin0-i), "-%llu", (long long unsigned)(step * scale));
			CPRINT((bin0+i), "%llu", (long long unsigned)(step * scale));
		}
		scale *= factor;
	}
	scale = (uint64_t)powl(factor, hmag + 1);
	CPRINT(0 , "<= -%llu", (long long unsigned)scale);
	CPRINT(nbins-1, ">= %llu", (long long unsigned)scale);
#undef CPRINT

	/* deal with the ghost-bin problem */
	if (pad > 0) {
		int j;
		/* first_bin */
		j = cwidth * (first_bin - first_bin + pad);
		if (c[j] == 0) {
			for (i = 1; i < pad; i++)
				if (c[j-cwidth*i] != 0)
					break;
			if (i < pad) /* should be true */
				memcpy(c+j, c+j-cwidth*i, cwidth);
		}
		/* last_bin */
		j = cwidth * (last_bin - first_bin + pad);
		if (c[j] == 0) {
			for (i = 1; i < pad; i++)
				if (c[j+cwidth*i] != 0)
					break;
			if (i < pad) /* should be true */
				memcpy(c+j, c+j+cwidth*i, cwidth);
		}
	}

	/* now print */
	for (i = first_bin; i <= last_bin; i++) {
		if (c[cwidth*(i-first_bin+pad)] == 0)
			continue;
		err = dt_printf(dtp, fp, "%16s ", c + cwidth*(i-first_bin+pad));
		if (err < 0 || dt_print_quantline(dtp, fp, data[i], normal,
		    total, positives, negatives) < 0)
			return -1;
	}

	return 0;
}

/*ARGSUSED*/
static int
dt_print_average(dtrace_hdl_t *dtp, FILE *fp, caddr_t addr,
    size_t size, uint64_t normal)
{
	/* LINTED - alignment */
	int64_t *data = (int64_t *)addr;

	return dt_printf(dtp, fp, " %16lld", data[0] ?
	    (long long)(data[1] / (int64_t)normal / data[0]) : 0);
}

/*ARGSUSED*/
static int
dt_print_stddev(dtrace_hdl_t *dtp, FILE *fp, caddr_t addr,
    size_t size, uint64_t normal)
{
	/* LINTED - alignment */
	uint64_t *data = (uint64_t *)addr;

	return dt_printf(dtp, fp, " %16llu", data[0] ?
	    (unsigned long long)dt_stddev(data, normal) : 0);
}

static int
dt_print_rawbytes(dtrace_hdl_t *dtp, FILE *fp, caddr_t addr, size_t nbytes)
{
	int i, j, margin = 5;
	char *c = (char *)addr;

	if (nbytes == 0)
		return 0;

	if (dt_printf(dtp, fp, "\n%*s      ", margin, "") < 0)
		return -1;

	for (i = 0; i < 16; i++)
		if (dt_printf(dtp, fp, "  %c", "0123456789abcdef"[i]) < 0)
			return -1;

	if (dt_printf(dtp, fp, "  0123456789abcdef\n") < 0)
		return -1;

	for (i = 0; i < nbytes; i += 16) {
		if (dt_printf(dtp, fp, "%*s%5x:", margin, "", i) < 0)
			return -1;

		for (j = i; j < i + 16 && j < nbytes; j++) {
			if (dt_printf(dtp, fp, " %02x", (uchar_t)c[j]) < 0)
				return -1;
		}

		while (j++ % 16) {
			if (dt_printf(dtp, fp, "   ") < 0)
				return -1;
		}

		if (dt_printf(dtp, fp, "  ") < 0)
			return -1;

		for (j = i; j < i + 16 && j < nbytes; j++) {
			if (dt_printf(dtp, fp, "%c",
			    c[j] < ' ' || c[j] > '~' ? '.' : c[j]) < 0)
				return -1;
		}

		if (dt_printf(dtp, fp, "\n") < 0)
			return -1;
	}

	return 0;
}

/*ARGSUSED*/
int
dt_print_bytes(dtrace_hdl_t *dtp, FILE *fp, caddr_t addr,
    size_t nbytes, int width, int quiet)
{
	/*
	 * If the byte stream is a series of printable characters, followed by
	 * a terminating byte, we print it out as a string.  Otherwise, we
	 * assume that it's something else and just print the bytes.
	 */
	int i, j;
	char *c = (char *)addr;

	if (nbytes == 0)
		return 0;

	if (dtp->dt_options[DTRACEOPT_RAWBYTES] != DTRACEOPT_UNSET)
		return dt_print_rawbytes(dtp, fp, addr, nbytes);

	for (i = 0; i < nbytes; i++) {
		/*
		 * We define a "printable character" to be one for which
		 * isprint(3C) returns non-zero, isspace(3C) returns non-zero,
		 * or a character which is either backspace or the bell.
		 * Backspace and the bell are regrettably special because
		 * they fail the first two tests -- and yet they are entirely
		 * printable.  These are the only two control characters that
		 * have meaning for the terminal and for which isprint(3C) and
		 * isspace(3C) return 0.
		 */
		if (isprint(c[i]) || isspace(c[i]) ||
		    c[i] == '\b' || c[i] == '\a')
			continue;

		if (c[i] == '\0' && i > 0) {
			/*
			 * This looks like it might be a string.  Before we
			 * assume that it is indeed a string, check the
			 * remainder of the byte range; if it contains
			 * additional non-nul characters, we'll assume that
			 * it's a binary stream that just happens to look like
			 * a string, and we'll print out the individual bytes.
			 */
			for (j = i + 1; j < nbytes; j++) {
				if (c[j] != '\0')
					break;
			}

			if (j != nbytes)
				break;

			if (quiet)
				return dt_printf(dtp, fp, "%s", c);
			else
				return dt_printf(dtp, fp, "  %-*s", width, c);
		}

		break;
	}

	if (i == nbytes) {
		/*
		 * The byte range is all printable characters, but there is
		 * no trailing nul byte.  We'll assume that it's a string and
		 * print it as such.
		 */
		char *s = alloca(nbytes + 1);
		memcpy(s, c, nbytes);
		s[nbytes] = '\0';
		return dt_printf(dtp, fp, "  %-*s", width, s);
	}

	/* print the bytes raw */
	return dt_print_rawbytes(dtp, fp, addr, nbytes);
}

static int
dt_print_tracemem(dtrace_hdl_t *dtp, FILE *fp, const dtrace_recdesc_t *rec,
    uint_t nrecs, const caddr_t buf)
{
	uint64_t arg = rec->dtrd_arg;
	caddr_t addr = buf + rec->dtrd_offset;
	size_t size = rec->dtrd_size;
	unsigned int nconsumed = 1;

	if (arg == DTRACE_TRACEMEM_DYNAMIC) {
		const dtrace_recdesc_t *drec;
		uint64_t darg;
		uint64_t dsize;
		int dpositive;

		if (nrecs < 2)
			return dt_set_errno(dtp, EDT_TRACEMEM);

		drec = rec + 1;
		darg = drec->dtrd_arg;

		if (drec->dtrd_action != DTRACEACT_TRACEMEM ||
		    (darg != DTRACE_TRACEMEM_SIZE &&
		    darg != DTRACE_TRACEMEM_SSIZE))
			return dt_set_errno(dtp, EDT_TRACEMEM);

		if (dt_read_scalar(buf, drec, &dsize) < 0)
			return dt_set_errno(dtp, EDT_TRACEMEM);

		dpositive = drec->dtrd_arg == DTRACE_TRACEMEM_SIZE ||
		    (dsize & (1 << (drec->dtrd_size * NBBY - 1))) == 0;

		if (dpositive && dsize < size)
			size = (size_t)dsize;

		nconsumed++;
	} else if (arg != DTRACE_TRACEMEM_STATIC) {
		return dt_set_errno(dtp, EDT_TRACEMEM);
	}

	if (dt_print_rawbytes(dtp, fp, addr, size) < 0)
		return -1;

	return nconsumed;
}

int
dt_print_stack(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    caddr_t addr, int depth, int size)
{
	dtrace_syminfo_t dts;
	GElf_Sym sym;
	int i, indent;
	char c[PATH_MAX * 2];
	uint64_t pc;

	if (dt_printf(dtp, fp, "\n") < 0)
		return -1;

	if (format == NULL)
		format = "%s";

	if (dtp->dt_options[DTRACEOPT_STACKINDENT] != DTRACEOPT_UNSET)
		indent = (int)dtp->dt_options[DTRACEOPT_STACKINDENT];
	else
		indent = _dtrace_stkindent;

	for (i = 0; i < depth; i++) {
		switch (size) {
		case sizeof(uint32_t):
			/* LINTED - alignment */
			pc = *((uint32_t *)addr);
			break;

		case sizeof(uint64_t):
			/* LINTED - alignment */
			pc = *((uint64_t *)addr);
			break;

		default:
			return dt_set_errno(dtp, EDT_BADSTACKPC);
		}

		if (pc == 0)
			break;

		addr += size;

		if (dt_printf(dtp, fp, "%*s", indent, "") < 0)
			return -1;

		if (dtrace_lookup_by_addr(dtp, pc, &sym, &dts) == 0) {
			if (pc > sym.st_value)
				snprintf(c, sizeof(c), "%s`%s+0x%llx",
					 dts.object, dts.name,
					 (long long unsigned)pc - sym.st_value);
			else
				snprintf(c, sizeof(c), "%s`%s",
					 dts.object, dts.name);
		} else {
			/*
			 * We'll repeat the lookup, but this time we'll specify
			 * a NULL GElf_Sym -- indicating that we're only
			 * interested in the containing module.
			 */
			if (dtrace_lookup_by_addr(dtp, pc, NULL, &dts) == 0)
				snprintf(c, sizeof(c), "%s`0x%llx",
					 dts.object, (long long unsigned)pc);
			else
				snprintf(c, sizeof(c), "0x%llx",
				    (long long unsigned)pc);
		}

		if (dt_printf(dtp, fp, format, c) < 0)
			return -1;

		if (dt_printf(dtp, fp, "\n") < 0)
			return -1;
	}

	return 0;
}

int
dt_print_ustack(dtrace_hdl_t *dtp, FILE *fp, const char *format,
    caddr_t addr, uint64_t arg)
{
	/* LINTED - alignment */
	uint64_t *pc = ((uint64_t *)addr);
	uint32_t depth = DTRACE_USTACK_NFRAMES(arg);
	uint32_t strsize = DTRACE_USTACK_STRSIZE(arg);
	const char *strbase = addr + (depth + 1) * sizeof(uint64_t);
	const char *str = strsize ? strbase : NULL;
	int err = 0;

	const char *name;
	char objname[PATH_MAX], c[PATH_MAX * 2];
	GElf_Sym sym;
	int i, indent;
	pid_t pid = -1, tgid;

	if (depth == 0)
		return 0;

	tgid = (pid_t)*pc++;

	if (dt_printf(dtp, fp, "\n") < 0)
		return -1;

	if (format == NULL)
		format = "%s";

	if (dtp->dt_options[DTRACEOPT_STACKINDENT] != DTRACEOPT_UNSET)
		indent = (int)dtp->dt_options[DTRACEOPT_STACKINDENT];
	else
		indent = _dtrace_stkindent;

	/*
	 * Ultimately, we need to add an entry point in the library vector for
	 * determining <symbol, offset> from <tgid, address>.  For now, if
	 * this is a vector open, we just print the raw address or string.
	 */
	if (dtp->dt_vector == NULL)
		pid = dt_proc_grab_lock(dtp, tgid, DTRACE_PROC_WAITING |
		    DTRACE_PROC_SHORTLIVED);

	for (i = 0; i < depth && pc[i] != 0; i++) {
		const prmap_t *map;

		if ((err = dt_printf(dtp, fp, "%*s", indent, "")) < 0)
			break;
		if (dtp->dt_options[DTRACEOPT_NORESOLVE] != DTRACEOPT_UNSET
		    && pid >= 0) {
			if (dt_Pobjname(dtp, pid, pc[i], objname,
			    sizeof(objname)) != NULL) {
				const prmap_t *pmap = NULL;
				uint64_t offset = pc[i];

				pmap = dt_Paddr_to_map(dtp, pid, pc[i]);

				if (pmap)
					offset = pc[i] - pmap->pr_vaddr;

				snprintf(c, sizeof(c), "%s:0x%llx",
				    dt_basename(objname), (unsigned long long)offset);

			} else
				snprintf(c, sizeof(c), "0x%llx",
				    (unsigned long long)pc[i]);

		} else if (pid >= 0 && dt_Plookup_by_addr(dtp, pid, pc[i],
							  &name, &sym) == 0) {
			if (dt_Pobjname(dtp, pid, pc[i], objname,
					sizeof(objname)) != NULL) {
				if (pc[i] > sym.st_value)
					snprintf(c, sizeof(c), "%s`%s+0x%llx",
						 dt_basename(objname), name,
						 (unsigned long long)(pc[i] - sym.st_value));
				else
					snprintf(c, sizeof(c), "%s`%s",
						 dt_basename(objname), name);
			} else
				snprintf(c, sizeof(c), "0x%llx",
				    (unsigned long long)pc[i]);
			/* Allocated by Plookup_by_addr. */
			free((char *)name);
		} else if (str != NULL && str[0] != '\0' && str[0] != '@' &&
		    (pid >= 0 &&
			((map = dt_Paddr_to_map(dtp, pid, pc[i])) == NULL ||
			    (map->pr_mflags & MA_WRITE)))) {
			/*
			 * If the current string pointer in the string table
			 * does not point to an empty string _and_ the program
			 * counter falls in a writable region, we'll use the
			 * string from the string table instead of the raw
			 * address.  This last condition is necessary because
			 * some (broken) ustack helpers will return a string
			 * even for a program counter that they can't
			 * identify.  If we have a string for a program
			 * counter that falls in a segment that isn't
			 * writable, we assume that we have fallen into this
			 * case and we refuse to use the string.
			 */
			snprintf(c, sizeof(c), "%s", str);
		} else {
			if (pid >= 0 && dt_Pobjname(dtp, pid, pc[i], objname,
			    sizeof(objname)) != NULL)
				snprintf(c, sizeof(c), "%s`0x%llx",
				    dt_basename(objname), (unsigned long long)pc[i]);
			else
				snprintf(c, sizeof(c), "0x%llx",
				    (unsigned long long)pc[i]);
		}

		if ((err = dt_printf(dtp, fp, format, c)) < 0)
			break;

		if ((err = dt_printf(dtp, fp, "\n")) < 0)
			break;

		if (str != NULL && str[0] == '@') {
			/*
			 * If the first character of the string is an "at" sign,
			 * then the string is inferred to be an annotation --
			 * and it is printed out beneath the frame and offset
			 * with brackets.
			 */
			if ((err = dt_printf(dtp, fp, "%*s", indent, "")) < 0)
				break;

			snprintf(c, sizeof(c), "  [ %s ]", &str[1]);

			if ((err = dt_printf(dtp, fp, format, c)) < 0)
				break;

			if ((err = dt_printf(dtp, fp, "\n")) < 0)
				break;
		}

		if (str != NULL) {
			str += strlen(str) + 1;
			if (str - strbase >= strsize)
				str = NULL;
		}
	}

	if (pid >= 0)
		dt_proc_release_unlock(dtp, pid);

	return err;
}

static int
dt_print_usym(dtrace_hdl_t *dtp, FILE *fp, caddr_t addr, dtrace_actkind_t act)
{
	/* LINTED - alignment */
	uint64_t tgid = ((uint64_t *)addr)[0];
	/* LINTED - alignment */
	uint64_t pc = ((uint64_t *)addr)[1];
	const char *format = "  %-50s";
	char *s;
	int n, len = 256;

	if (act == DTRACEACT_USYM && dtp->dt_vector == NULL) {
		pid_t pid;

		pid = dt_proc_grab_lock(dtp, tgid, DTRACE_PROC_WAITING |
		    DTRACE_PROC_SHORTLIVED);
		if (pid >= 0) {
			GElf_Sym sym;

			if (dt_Plookup_by_addr(dtp, pid, pc, NULL, &sym) == 0)
				pc = sym.st_value;

			dt_proc_release_unlock(dtp, pid);
		}
	}

	do {
		n = len;
		s = alloca(n);
	} while ((len = dtrace_uaddr2str(dtp, tgid, pc, s, n)) > n);

	return dt_printf(dtp, fp, format, s);
}

int
dt_print_umod(dtrace_hdl_t *dtp, FILE *fp, const char *format, caddr_t addr)
{
	/* LINTED - alignment */
	uint64_t tgid = ((uint64_t *)addr)[0];
	/* LINTED - alignment */
	uint64_t pc = ((uint64_t *)addr)[1];
	int err = 0;

	char objname[PATH_MAX], c[PATH_MAX * 2];
	pid_t pid = -1;

	if (format == NULL)
		format = "  %-50s";

	/*
	 * See the comment in dt_print_ustack() for the rationale for
	 * printing raw addresses in the vectored case.
	 */
	if (dtp->dt_vector == NULL)
		pid = dt_proc_grab_lock(dtp, tgid, DTRACE_PROC_WAITING |
				 DTRACE_PROC_SHORTLIVED);

	if (pid >= 0 && dt_Pobjname(dtp, pid, pc, objname,
		sizeof(objname)) != NULL)
		snprintf(c, sizeof(c), "%s", dt_basename(objname));
	else
		snprintf(c, sizeof(c), "0x%llx", (unsigned long long)pc);

	err = dt_printf(dtp, fp, format, c);

	if (pid >= 0)
		dt_proc_release_unlock(dtp, pid);

	return err;
}

static int
dt_print_sym(dtrace_hdl_t *dtp, FILE *fp, const char *format, caddr_t addr)
{
	/* LINTED - alignment */
	uint64_t pc = *((uint64_t *)addr);
	dtrace_syminfo_t dts;
	GElf_Sym sym;
	char c[PATH_MAX * 2];

	if (format == NULL)
		format = "  %-50s";

	if (dtrace_lookup_by_addr(dtp, pc, &sym, &dts) == 0) {
		snprintf(c, sizeof(c), "%s`%s", dts.object, dts.name);
	} else {
		/*
		 * We'll repeat the lookup, but this time we'll specify a
		 * NULL GElf_Sym -- indicating that we're only interested in
		 * the containing module.
		 */
		if (dtrace_lookup_by_addr(dtp, pc, NULL, &dts) == 0)
			snprintf(c, sizeof(c), "%s`0x%llx",
				 dts.object, (unsigned long long)pc);
		else
			snprintf(c, sizeof(c), "0x%llx", (unsigned long long)pc);
	}

	if (dt_printf(dtp, fp, format, c) < 0)
		return -1;

	return 0;
}

int
dt_print_mod(dtrace_hdl_t *dtp, FILE *fp, const char *format, caddr_t addr)
{
	/* LINTED - alignment */
	uint64_t pc = *((uint64_t *)addr);
	dtrace_syminfo_t dts;
	char c[PATH_MAX * 2];

	if (format == NULL)
		format = "  %-50s";

	if (dtrace_lookup_by_addr(dtp, pc, NULL, &dts) == 0)
		snprintf(c, sizeof(c), "%s", dts.object);
	else
		snprintf(c, sizeof(c), "0x%llx", (unsigned long long)pc);

	if (dt_printf(dtp, fp, format, c) < 0)
		return -1;

	return 0;
}

int
dt_print_pcap(dtrace_hdl_t *dtp, FILE *fp, dtrace_recdesc_t *rec, uint_t nrecs,
	      const caddr_t buf)
{
	caddr_t			data;
	uint64_t		time, proto, pktlen;
	uint64_t		maxlen = dtp->dt_options[DTRACEOPT_PCAPSIZE];
	const char		*filename;

	if (nrecs < 4)
		return dt_set_errno(dtp, EDT_PCAP);

	if (dt_read_scalar(buf, rec, &time) < 0 ||
	    dt_read_scalar(buf, rec + 1, &pktlen) < 0)
		return dt_set_errno(dtp, EDT_PCAP);

	/* If pktlen is 0, the skb must have been NULL, so don't capture it. */
	if (pktlen == 0)
		return 0;

	if (dt_read_scalar(buf, rec + 2, &proto) < 0)
		return dt_set_errno(dtp, EDT_PCAP);

	/*
	 * Dump pcap data to dump file if handler command/output file is
	 * specified or if we have been able to connect a pipe to tshark,
	 * otherwise print tracemem-like output.
	 */
	data = buf + (rec + 3)->dtrd_offset;
	filename = dt_pcap_filename(dtp, fp);
	if (filename != NULL)
		dt_pcap_dump(dtp, filename, proto, time, data,
			     (uint32_t)pktlen, (uint32_t)maxlen);
	else if (dt_print_rawbytes(dtp, fp, data,
				   pktlen > maxlen ? maxlen : pktlen) < 0)
		return -1;

	return 4;
}

/*
 * This function is also used to denormalize aggregations, because that is
 * equivalent to normalizing them using normal = 1.
 */
static int
dt_normalize(dtrace_hdl_t *dtp, caddr_t base, dtrace_recdesc_t *rec)
{
	int		act = rec->dtrd_arg;
	dtrace_aggid_t	id;
	uint64_t	normal;
	caddr_t		addr;

	/*
	 * We (should) have two records:  the aggregation ID followed by the
	 * normalization value.
	 */
	addr = base + rec->dtrd_offset;

	if (rec->dtrd_size != sizeof(dtrace_aggid_t))
		return dt_set_errno(dtp, EDT_BADNORMAL);

	id = *((dtrace_aggid_t *)addr);

	if (act == DT_ACT_NORMALIZE) {
		rec++;

		if (rec->dtrd_action != DTRACEACT_LIBACT)
			return dt_set_errno(dtp, EDT_BADNORMAL);

		if (rec->dtrd_arg != DT_ACT_NORMALIZE)
			return dt_set_errno(dtp, EDT_BADNORMAL);

		addr = base + rec->dtrd_offset;

		switch (rec->dtrd_size) {
		case sizeof(uint64_t):
			normal = *((uint64_t *)addr);
			break;
		case sizeof(uint32_t):
			normal = *((uint32_t *)addr);
			break;
		case sizeof(uint16_t):
			normal = *((uint16_t *)addr);
			break;
		case sizeof(uint8_t):
			normal = *((uint8_t *)addr);
			break;
		default:
			return dt_set_errno(dtp, EDT_BADNORMAL);
		}
	} else
		normal = 1;

	dtp->dt_adesc[id]->dtagd_normal = normal;

	return 0;
}

static int
dt_clear(dtrace_hdl_t *dtp, caddr_t base, dtrace_recdesc_t *rec)
{
	dtrace_aggid_t	aid;
	uint64_t	gen;
	caddr_t		addr;

	/* We have just one record: the aggregation ID. */
	addr = base + rec->dtrd_offset;

	if (rec->dtrd_size != sizeof(dtrace_aggid_t))
		return dt_set_errno(dtp, EDT_BADNORMAL);

	aid = *((dtrace_aggid_t *)addr);

	if (dt_bpf_map_lookup(dtp->dt_genmap_fd, &aid, &gen) < 0)
		return -1;
	gen++;
	if (dt_bpf_map_update(dtp->dt_genmap_fd, &aid, &gen) < 0)
		return -1;

	/* Also clear our own copy of the data, in case it gets printed. */
	dtrace_aggregate_walk(dtp, dt_aggregate_clear_one, dtp);

	return 0;
}

typedef struct dt_trunc {
	dtrace_aggid_t	dttd_id;
	uint64_t	dttd_remaining;
} dt_trunc_t;

static int
dt_trunc_agg(const dtrace_aggdata_t *aggdata, void *arg)
{
	dt_trunc_t		*trunc = arg;
	dtrace_aggdesc_t	*agg = aggdata->dtada_desc;
	dtrace_aggid_t		id = trunc->dttd_id;

	if (agg->dtagd_nkrecs == 0)
		return DTRACE_AGGWALK_NEXT;

	if (agg->dtagd_varid != id)
		return DTRACE_AGGWALK_NEXT;

	if (trunc->dttd_remaining == 0)
		return DTRACE_AGGWALK_REMOVE;

	trunc->dttd_remaining--;
	return DTRACE_AGGWALK_NEXT;
}

static int
dt_trunc(dtrace_hdl_t *dtp, caddr_t base, dtrace_recdesc_t *rec)
{
	dt_trunc_t trunc;
	caddr_t addr;
	int64_t remaining;
	int (*func)(dtrace_hdl_t *, dtrace_aggregate_f *, void *);

	/*
	 * We (should) have two records:  the aggregation ID followed by the
	 * number of aggregation entries after which the aggregation is to be
	 * truncated.
	 */
	addr = base + rec->dtrd_offset;

	if (rec->dtrd_size != sizeof(dtrace_aggid_t))
		return dt_set_errno(dtp, EDT_BADTRUNC);

	/* LINTED - alignment */
	trunc.dttd_id = *((dtrace_aggid_t *)addr);
	rec++;

	if (rec->dtrd_action != DTRACEACT_LIBACT)
		return dt_set_errno(dtp, EDT_BADTRUNC);

	if (rec->dtrd_arg != DT_ACT_TRUNC)
		return dt_set_errno(dtp, EDT_BADTRUNC);

	addr = base + rec->dtrd_offset;

	switch (rec->dtrd_size) {
	case sizeof(int64_t):
		/* LINTED - alignment */
		remaining = *((int64_t *)addr);
		break;
	case sizeof(int32_t):
		/* LINTED - alignment */
		remaining = *((int32_t *)addr);
		break;
	case sizeof(int16_t):
		/* LINTED - alignment */
		remaining = *((int16_t *)addr);
		break;
	case sizeof(int8_t):
		remaining = *((int8_t *)addr);
		break;
	default:
		return dt_set_errno(dtp, EDT_BADNORMAL);
	}

	if (remaining < 0) {
		func = dtrace_aggregate_walk_valsorted;
		remaining = -remaining;
	} else
		func = dtrace_aggregate_walk_valrevsorted;

	assert(remaining >= 0);
	trunc.dttd_remaining = remaining;

	func(dtp, dt_trunc_agg, &trunc);

	return 0;
}

static int
dt_print_datum(dtrace_hdl_t *dtp, FILE *fp, dtrace_recdesc_t *rec,
	       caddr_t addr, uint64_t normal, uint64_t sig)
{
	int			err;
	dtrace_actkind_t	act = rec->dtrd_action;
	size_t			size = rec->dtrd_size;

	/* Apply the record offset to the base address. */
	addr += rec->dtrd_offset;

	switch (act) {
	case DTRACEACT_STACK:
		return dt_print_stack(dtp, fp, NULL, addr, rec->dtrd_arg,
				      rec->dtrd_size / rec->dtrd_arg);

	case DTRACEACT_USTACK:
	case DTRACEACT_JSTACK:
		return dt_print_ustack(dtp, fp, NULL, addr, rec->dtrd_arg);

	case DTRACEACT_USYM:
	case DTRACEACT_UADDR:
		return dt_print_usym(dtp, fp, addr, act);

	case DTRACEACT_UMOD:
		return dt_print_umod(dtp, fp, NULL, addr);

	case DTRACEACT_SYM:
		return dt_print_sym(dtp, fp, NULL, addr);

	case DTRACEACT_MOD:
		return dt_print_mod(dtp, fp, NULL, addr);

	case DT_AGG_QUANTIZE:
		return dt_print_quantize(dtp, fp, addr, size, normal);

	case DT_AGG_LQUANTIZE:
		return dt_print_lquantize(dtp, fp, addr, size, normal, sig);

	case DT_AGG_LLQUANTIZE:
		return dt_print_llquantize(dtp, fp, addr, size, normal, sig);

	case DT_AGG_AVG:
		return dt_print_average(dtp, fp, addr, size, normal);

	case DT_AGG_STDDEV:
		return dt_print_stddev(dtp, fp, addr, size, normal);

	default:
		break;
	}

	switch (size) {
	case sizeof(uint64_t):
		err = dt_printf(dtp, fp, " %16lld",
		    /* LINTED - alignment */
		    (long long)*((uint64_t *)addr) / normal);
		break;
	case sizeof(uint32_t):
		/* LINTED - alignment */
		err = dt_printf(dtp, fp, " %8d", *((uint32_t *)addr) /
		    (uint32_t)normal);
		break;
	case sizeof(uint16_t):
		/* LINTED - alignment */
		err = dt_printf(dtp, fp, " %5d", *((uint16_t *)addr) /
		    (uint32_t)normal);
		break;
	case sizeof(uint8_t):
		err = dt_printf(dtp, fp, " %3d", *((uint8_t *)addr) /
		    (uint32_t)normal);
		break;
	default:
		err = dt_print_bytes(dtp, fp, addr, size, 50, 0);
		break;
	}

	return err;
}

static int
dt_print_aggs(const dtrace_aggdata_t **aggsdata, int naggvars, void *arg)
{
	int			i;
	dtrace_print_aggdata_t	*pd = arg;
	const dtrace_aggdata_t	*aggdata = aggsdata[0];
	dtrace_aggdesc_t	*agg = aggdata->dtada_desc;
	FILE			*fp = pd->dtpa_fp;
	dtrace_hdl_t		*dtp = pd->dtpa_dtp;
	dtrace_recdesc_t	*rec;

	/*
	 * Iterate over each record description in the key, printing the traced
	 * data.  The first record is skipped because it holds the variable ID.
	 */
	for (i = 1; i < agg->dtagd_nkrecs; i++) {
		rec = &agg->dtagd_krecs[i];

		if (dt_print_datum(dtp, fp, rec, aggdata->dtada_key, 1, 0) < 0)
			return DTRACE_AGGWALK_ERROR;

		if (dt_buffered_flush(dtp, NULL, rec, aggdata,
				      DTRACE_BUFDATA_AGGKEY) < 0)
			return DTRACE_AGGWALK_ERROR;
	}

	for (i = (naggvars == 1 ? 0 : 1); i < naggvars; i++) {
		uint64_t	normal;

		aggdata = aggsdata[i];
		agg = aggdata->dtada_desc;
		rec = &agg->dtagd_drecs[DT_AGGDATA_RECORD];

		assert(DTRACEACT_ISAGG(rec->dtrd_action));
		normal = aggdata->dtada_desc->dtagd_normal;

		if (dt_print_datum(dtp, fp, rec, aggdata->dtada_data, normal,
				   agg->dtagd_sig) < 0)
			return DTRACE_AGGWALK_ERROR;
		if (aggdata->dtada_percpu != NULL) {
			int j, max_cpus = aggdata->dtada_hdl->dt_conf.max_cpuid + 1;
			for (j = 0; j < max_cpus; j++) {
				if (dt_printf(dtp, fp, "\n    [CPU %d]", aggdata->dtada_hdl->dt_conf.cpus[j].cpu_id) < 0)
					return DTRACE_AGGWALK_ERROR;
				if (dt_print_datum(dtp, fp, rec, aggdata->dtada_percpu[j], normal, agg->dtagd_sig) < 0)
					return DTRACE_AGGWALK_ERROR;
			}
		}

		if (dt_buffered_flush(dtp, NULL, rec, aggdata,
				      DTRACE_BUFDATA_AGGVAL) < 0)
			return DTRACE_AGGWALK_ERROR;

		if (!pd->dtpa_allunprint)
			agg->dtagd_flags |= DTRACE_AGD_PRINTED;
	}

	if (dt_printf(dtp, fp, "\n") < 0)
		return DTRACE_AGGWALK_ERROR;

	if (dt_buffered_flush(dtp, NULL, NULL, aggdata,
			      DTRACE_BUFDATA_AGGFORMAT |
			      DTRACE_BUFDATA_AGGLAST) < 0)
		return DTRACE_AGGWALK_ERROR;

	return DTRACE_AGGWALK_NEXT;
}

int
dt_print_agg(const dtrace_aggdata_t *aggdata, void *arg)
{
	dtrace_print_aggdata_t	*pd = arg;
	dtrace_aggdesc_t	*agg = aggdata->dtada_desc;
	dtrace_aggid_t		aggvarid = pd->dtpa_id;

	if (pd->dtpa_allunprint) {
		if (agg->dtagd_flags & DTRACE_AGD_PRINTED)
			return DTRACE_AGGWALK_NEXT;
	} else {
		/*
		 * If we're not printing all unprinted aggregations, then the
		 * aggregation variable ID denotes a specific aggregation
		 * variable that we should print -- skip any other aggregations
		 * that we encounter.
		 */
		if (agg->dtagd_nkrecs == 0)
			return DTRACE_AGGWALK_NEXT;

		if (aggvarid != agg->dtagd_varid)
			return DTRACE_AGGWALK_NEXT;
	}

	return dt_print_aggs(&aggdata, 1, arg);
}

static int
dt_printa(dtrace_hdl_t *dtp, FILE *fp, void *fmtdata,
	  const dtrace_probedata_t *data, const dtrace_recdesc_t *recs,
	  uint_t nrecs, const void *buf, size_t len)
{
	dtrace_print_aggdata_t	pd;
	dtrace_aggid_t		*aggvars;
	int			i, naggvars = 0;

	aggvars = alloca(nrecs * sizeof(dtrace_aggid_t));

	/*
	 * This might be a printa() with multiple aggregation variables.  We
	 * need to scan forward through the records until we find a record that
	 * does not belong to this printa() statement.
	 */
	for (i = 0; i < nrecs; i++) {
		const dtrace_recdesc_t *nrec = &recs[i];

		if (nrec->dtrd_arg != recs->dtrd_arg)
			break;

		if (nrec->dtrd_action != recs->dtrd_action)
			return dt_set_errno(dtp, EDT_BADAGG);

		aggvars[naggvars++] =
		    /* LINTED - alignment */
		    *((dtrace_aggid_t *)((caddr_t)buf + nrec->dtrd_offset));
	}

	if (naggvars == 0)
		return dt_set_errno(dtp, EDT_BADAGG);

	pd.dtpa_dtp = dtp;
	pd.dtpa_fp = fp;
	pd.dtpa_allunprint = 0;

	if (naggvars == 1) {
		pd.dtpa_id = aggvars[0];

		if (dt_printf(dtp, fp, "\n") < 0 ||
		    dtrace_aggregate_walk_sorted(dtp, dt_print_agg, &pd) < 0)
			return -1;		/* errno is set for us */
	} else {
		pd.dtpa_id = 0;

		if (dt_printf(dtp, fp, "\n") < 0 ||
		    dtrace_aggregate_walk_joined(dtp, aggvars, naggvars,
						 dt_print_aggs, &pd) < 0)
			return -1;		/* errno is set for us */
	}

	return i;
}

int
dt_setopt(dtrace_hdl_t *dtp, const dtrace_probedata_t *data,
    const char *option, const char *value)
{
	int len, rval;
	char *msg;
	const char *errstr;
	dtrace_setoptdata_t optdata;

	memset(&optdata, 0, sizeof(optdata));
	dtrace_getopt(dtp, option, &optdata.dtsda_oldval);

	if (dtrace_setopt(dtp, option, value) == 0) {
		dtrace_getopt(dtp, option, &optdata.dtsda_newval);
		optdata.dtsda_probe = data;
		optdata.dtsda_option = option;
		optdata.dtsda_handle = dtp;

		if ((rval = dt_handle_setopt(dtp, &optdata)) != 0)
			return rval;

		return 0;
	}

	errstr = dtrace_errmsg(dtp, dtrace_errno(dtp));
	len = strlen(option) + strlen(value) + strlen(errstr) + 80;
	msg = alloca(len);

	snprintf(msg, len, "couldn't set option \"%s\" to \"%s\": %s\n",
	    option, value, errstr);

	if ((rval = dt_handle_liberr(dtp, data, msg)) == 0)
		return 0;

	return rval;
}

static int
dt_print_trace(dtrace_hdl_t *dtp, FILE *fp, dtrace_recdesc_t *rec,
	       caddr_t data, int quiet)
{
	if (dtp->dt_options[DTRACEOPT_RAWBYTES] != DTRACEOPT_UNSET)
		return dt_print_rawbytes(dtp, fp, data, rec->dtrd_size);

	/*
	 * String data can be recognized as a non-scalar data item with
	 * alignment == 1.
	 * Any other non-scalar data items are printed as a byte stream.
	 */
	if (rec->dtrd_arg == DT_NF_REF) {
		char	*s = (char *)data;

		if (rec->dtrd_alignment > 1)
			return dt_print_rawbytes(dtp, fp, data, rec->dtrd_size);

		/* We have a string.  Print it. */
		if (quiet)
			return dt_printf(dtp, fp, "%s", s);
		else
			return dt_printf(dtp, fp, "  %-33s", s);
	}

	/*
	 * Differentiate between signed and unsigned numeric values.
	 *
	 * Note:
	 * This is an enhancement of the functionality present in DTrace v1,
	 * where values were anything smaller than a 64-bit value was used as a
	 * 32-bit value.  As such, (int8_t)-1 and (int16_t)-1 would be printed
	 * as 255 and 65535, but (int32_t)-1 and (int64_t) would be printed as
	 * -1.
	 */
	if (rec->dtrd_arg == DT_NF_SIGNED) {
		switch (rec->dtrd_size) {
		case sizeof(int64_t):
			return dt_printf(dtp, fp, quiet ? "%ld" : " %19ld",
				      *(int64_t *)data);
		case sizeof(int32_t):
			return dt_printf(dtp, fp, quiet ? "%d" : " %10d",
					 *(int32_t *)data);
		case sizeof(int16_t):
			return dt_printf(dtp, fp, quiet ? "%hd" : " %5hd",
					 *(int16_t *)data);
		case sizeof(int8_t):
			return dt_printf(dtp, fp, quiet ? "%hhd" : " %3hhd",
					 *(int8_t *)data);
		}
	} else {
		switch (rec->dtrd_size) {
		case sizeof(uint64_t):
			return dt_printf(dtp, fp, quiet ? "%lu" : " %20lu",
					 *(uint64_t *)data);
		case sizeof(uint32_t):
			return dt_printf(dtp, fp, quiet ? "%u" : " %10u",
					 *(uint32_t *)data);
		case sizeof(uint16_t):
			return dt_printf(dtp, fp, quiet ? "%hu" : " %5hu",
					 *(uint16_t *)data);
		case sizeof(uint8_t):
			return dt_printf(dtp, fp, quiet ? "%hhu" : " %3hhu",
					 *(uint8_t *)data);
		}
	}

	/*
	 * We should never get here, but if we do... print raw bytes.
	 */
	return dt_print_rawbytes(dtp, fp, data, rec->dtrd_size);
}

/*
 * The lifecycle of speculation buffers is as follows:
 *
 *  - They are created upon speculation() as entries in the specs map mapping
 *    from the speculation ID to a dt_bpf_specs_t entry with read, written,
 *    and draining values all zero.
 *
 *  - speculate() verifies the existence of the requested speculation entry in
 *    the specs map, and that draining has not been set in it, and atomically
 *    bumps the written value.
 *
 *  - Speculations drain from the perf ring buffer into possibly many
 *    dt_spec_buf_data instances, one dt_spec_buf_data per ring-buffer of data
 *    fetched from the kernel with a nonzero specid (one speculated clause);
 *    these are chained into dt_spec_bufs, one per speculation ID, and these are
 *    put into dtp->dt_spec_bufs instead of being consumed.
 *
 *  - commit / discard set the specs map entry's drained value to 1, which
 *    indicates that it is drainable by userspace and prevents further
 *    speculate()s, and  record a single entry in the output buffer with the
 *    committed/discarded ID attached
 *
 *  - Non-speculated probe buffers (with a zero specid) are scanned by
 *    dt_consume_one_probe for commit / discard; the first found for a given
 *    live speculation triggers draining, chaining the spec buf into
 *    dtp->dt_spec_bufs_draining.  dt_spec_bufs_draining contains a counter of
 *    the number of data buffers drained ("read", mirroring "written" in the
 *    specs map).
 *
 *  - dt_spec_bufs_draining is traversed on every buffer consume and any
 *    speculations that are committing have their speculation buffers processed
 *    as if they had just been received (but keeping their original CPU number);
 *    both entries that are committing and those that are discarding are then
 *    removed, leaving the buffer head behind to be filled out by future
 *    ring-buffer fetches, and the number removed recorded by incrementing
 *    dtsb_draining_t.dtsd_read.  Buffers for which read >= written are removed
 *    from dt_spec_bufs_draining and from dt_spec_bufs and have their ID removed
 *    from the specs map, freeing them for recycling by future calls to
 *    speculation().
 */

/*
 * Member of the dtsb_draining_list.
 */
typedef struct dtsb_draining {
	dt_list_t dtsd_list;
	dt_spec_buf_t *dtsd_dtsb;
	uint64_t dtsd_read;
} dtsb_draining_t;

static dt_spec_buf_t *
dt_spec_buf_create(dtrace_hdl_t *dtp, int32_t spec)
{
	dt_spec_buf_t *dtsb;

	dtsb = dt_zalloc(dtp, sizeof(struct dt_spec_buf));
	if (!dtsb)
		goto oom;
	dtsb->dtsb_id = spec;

	/* Needed for the htab destructor */
	dtsb->dtsb_dtp = dtp;

	if (dt_htab_insert(dtp->dt_spec_bufs, dtsb) < 0)
		goto oom;
	return dtsb;
oom:
	dt_free(dtp, dtsb);
	dt_set_errno(dtp, EDT_NOMEM);
	return NULL;
}

static dt_spec_buf_data_t *
dt_spec_buf_add_data(dtrace_hdl_t *dtp, dt_spec_buf_t *dtsb,
		     dtrace_epid_t epid, unsigned int cpu,
		     dtrace_datadesc_t *datadesc, char *data,
		     uint32_t size)
{
	dt_spec_buf_data_t *dsbd;

	dsbd = dt_zalloc(dtp, sizeof(struct dt_spec_buf_data));
	if (!dsbd)
		goto oom;

	dsbd->dsbd_cpu = cpu;
	dsbd->dsbd_size = size;
	dsbd->dsbd_data = dt_alloc(dtp, size);
	if (!dsbd->dsbd_data)
		goto oom;

	dtsb->dtsb_size += size;
	memcpy(dsbd->dsbd_data, data, size);

	dt_list_append(&dtsb->dtsb_dsbd_list, dsbd);
	return dsbd;

oom:
	dt_free(dtp, dsbd);
	dt_set_errno(dtp, EDT_NOMEM);
	return NULL;
}

/*
 * Remove a speculation buffer's data.  The buffer is left alone.
 */
static void
dt_spec_buf_data_destroy(dtrace_hdl_t *dtp, dt_spec_buf_t *dtsb)
{
	dt_spec_buf_data_t	*dsbd;

	while ((dsbd = dt_list_next(&dtsb->dtsb_dsbd_list)) != NULL) {

		dt_list_delete(&dtsb->dtsb_dsbd_list, dsbd);
		dt_free(dtp, dsbd->dsbd_data);
		dt_free(dtp, dsbd);
	}

	dtsb->dtsb_size = 0;
}

/*
 * Remove a speculation's buffers, the speculation itself, and the record of it
 * in the BPF specs map (which frees the ID for reuse).
 *
 * Note: if the speculation is being committed, it will also be interned on the
 * dtp->dt_spec_bufs_draining list.  Such entries must be removed first.
 */
static void
dt_spec_buf_destroy(dtrace_hdl_t *dtp, dt_spec_buf_t *dtsb)
{
	dt_ident_t *idp = dt_dlib_get_map(dtp, "specs");

	dt_spec_buf_data_destroy(dtp, dtsb);

	if (idp)
		dt_bpf_map_delete(idp->di_id, &dtsb->dtsb_id);
	dt_free(dtp, dtsb);
}

/*
 * Peeking flags (values for the peekflag parameter for functions that have
 * one).
 *
 * These let you process a single buffer more than once.  The first call
 * should pass CONSUME_PEEK_START: this suppresses deletion of consumed records.
 * Subsequent calls should pass CONSUME_PEEK; this does as CONSUME_PEEK_START
 * does, but also suppresses hiving off of speculative buffers (because they
 * have already been hived off under CONSUME_PEEK_START).  The final call should
 * pass CONSUME_PEEK_FINISH; this does a normal buffer consumption (with
 * deletion) except that speculative hiving-off is again suppressd.
 *
 * These are not bit-flags: pass only one.
 */
#define CONSUME_PEEK_START 0x1
#define CONSUME_PEEK 0x2
#define CONSUME_PEEK_FINISH 0x3

static dtrace_workstatus_t
dt_consume_one_probe(dtrace_hdl_t *dtp, FILE *fp, char *data, uint32_t size,
		     dtrace_probedata_t *pdat, dtrace_consume_probe_f *efunc,
		     dtrace_consume_rec_f *rfunc, int flow, int quiet,
		     int peekflags, dtrace_epid_t *last, int committing,
		     void *arg);

/*
 * Commit one speculation.
 */
static dtrace_workstatus_t
dt_commit_one_spec(dtrace_hdl_t *dtp, FILE *fp, dt_spec_buf_t *dtsb,
		   dtrace_probedata_t *pdat, dtrace_consume_probe_f *efunc,
		   dtrace_consume_rec_f *rfunc, int flow, int quiet,
		   int peekflags, dtrace_epid_t *last, void *arg)
{
	dt_spec_buf_data_t 	*dsbd;

	for (dsbd = dt_list_next(&dtsb->dtsb_dsbd_list);
	     dsbd != NULL; dsbd = dt_list_next(dsbd)) {
		dtrace_workstatus_t ret;
		dtrace_probedata_t specpdat;

		memcpy(&specpdat, pdat, sizeof(dtrace_probedata_t));
		specpdat.dtpda_cpu = dsbd->dsbd_cpu;
		ret = dt_consume_one_probe(dtp, fp, dsbd->dsbd_data,
					   dsbd->dsbd_size, &specpdat,
					   efunc, rfunc, flow, quiet,
					   peekflags, last, 1, arg);
		if (ret != DTRACE_WORKSTATUS_OKAY)
			return ret;
	}

	return DTRACE_WORKSTATUS_OKAY;
}

static dtrace_workstatus_t
dt_consume_one_probe(dtrace_hdl_t *dtp, FILE *fp, char *data, uint32_t size,
		     dtrace_probedata_t *pdat, dtrace_consume_probe_f *efunc,
		     dtrace_consume_rec_f *rfunc, int flow, int quiet,
		     int peekflags, dtrace_epid_t *last, int committing,
		     void *arg)
{
	dtrace_epid_t		epid;
	dtrace_datadesc_t	*epd;
	dt_spec_buf_t		tmpl;
	dt_spec_buf_t		*dtsb;
	int			specid;
	int			i;
	int			rval;
	dtrace_workstatus_t	ret;
	int			commit_discard_seen, only_commit_discards;
	int			data_recording = 1;

	epid = ((uint32_t *)data)[0];
	specid = ((uint32_t *)data)[1];

	/*
	 * Fill in the epid and address of the epid in the buffer.  We need to
	 * pass this to the efunc and possibly to create speculations.
	 */
	pdat->dtpda_epid = epid;
	pdat->dtpda_data = data;

	rval = dt_epid_lookup(dtp, epid, &pdat->dtpda_ddesc,
					 &pdat->dtpda_pdesc);
	if (rval != 0)
		return dt_set_errno(dtp, EDT_BADEPID);

	epd = pdat->dtpda_ddesc;
	if (epd->dtdd_uarg != DT_ECB_DEFAULT) {
		rval = dt_handle(dtp, pdat);

		if (rval == DTRACE_CONSUME_NEXT)
			return DTRACE_WORKSTATUS_OKAY;
		if (rval == DTRACE_CONSUME_ERROR)
			return DTRACE_WORKSTATUS_ERROR;
	}

	/*
	 * If speculating (and not peeking), hive this buffer off into a
	 * suitable spec buf, creating the spec buf head if need be.  No need to
	 * do anything else with this buffer until a commit or discard is seen
	 * for it (in some other, non-speculated buffer).
	 */

	if (!committing && specid != 0) {
		size_t cursz = 0;

		/*
		 * If peeking, we dealt with this buffer earlier: don't
		 * re-speculate it again.
		 */
		if (peekflags == CONSUME_PEEK ||
		    peekflags == CONSUME_PEEK_FINISH)
			return DTRACE_WORKSTATUS_OKAY;

		tmpl.dtsb_id = specid;
		dtsb = dt_htab_lookup(dtp->dt_spec_bufs, &tmpl);

		/*
		 * Discard when over the specsize.
		 *
		 * TODO: add a drop when OOM or > specsize -- and also on OOM in
		 * any of the consuming functions.
		 */

		if (dtsb)
			cursz = dtsb->dtsb_size;

		if (cursz + size > dtp->dt_options[DTRACEOPT_SPECSIZE]) {
			dtp->dt_specdrops++;
			return DTRACE_WORKSTATUS_OKAY;
		}

		if (!dtsb) {
			if ((dtsb = dt_spec_buf_create(dtp, specid)) == NULL) {
				dtp->dt_specdrops++;
				return DTRACE_WORKSTATUS_OKAY;
			}
		}

		if (dt_spec_buf_add_data(dtp, dtsb, epid, pdat->dtpda_cpu, epd,
					 data, size) == NULL)
			dtp->dt_specdrops++;

		return DTRACE_WORKSTATUS_OKAY;
	}

	/*
	 * First, scan for commit/discard.  Track whether we have seen discards,
	 * and whether we have seen anything else, to determine whether this
	 * clause should be considered data-recording from the user's
	 * perspective.  (A clause with only a discard is not data-recording.
	 * Commits cannot share a clause with data-recording actions at all: see
	 * dt_cg_clsflags.)
	 *
	 * Do nothing of this if committing speculated buffers, since speculated
	 * buffers cannot contain commits or discards.
	 */
	commit_discard_seen = 0;
	only_commit_discards = 1;
	for (i = 0; !committing && i < epd->dtdd_nrecs; i++) {
		dtrace_recdesc_t	*rec;
		dtrace_actkind_t	act;

		rec = &epd->dtdd_recs[i];
		act = rec->dtrd_action;

		if (act == DTRACEACT_COMMIT || act == DTRACEACT_DISCARD) {
			commit_discard_seen = 1;
		} else
			only_commit_discards = 0;
	}

	/*
	 * If this clause is a commit or discard, and no other actions have been
	 * seen, this is not a data-recording clause, and we should not call the
	 * efunc or rfunc at all.
	 */

	if (commit_discard_seen && only_commit_discards)
		data_recording = 0;

	if (data_recording) {
		if (flow)
			dt_flowindent(dtp, pdat, *last, DTRACE_EPIDNONE);

		rval = (*efunc)(pdat, arg);

		if (flow) {
			if (pdat->dtpda_flow == DTRACEFLOW_ENTRY)
				pdat->dtpda_indent += 2;
		}

		switch (rval) {
		case DTRACE_CONSUME_NEXT:
			return DTRACE_WORKSTATUS_OKAY;
		case DTRACE_CONSUME_DONE:
			return DTRACE_WORKSTATUS_DONE;
		case DTRACE_CONSUME_ABORT:
			return dt_set_errno(dtp, EDT_DIRABORT);
		case DTRACE_CONSUME_THIS:
			break;
		default:
			return dt_set_errno(dtp, EDT_BADRVAL);
		}
	}

	/*
	 * Now scan for data-recording actions.
	 *
	 * FIXME: This code is temporary.
	 */
	for (i = 0; i < epd->dtdd_nrecs; i++) {
		int			n;
		dtrace_recdesc_t	*rec;
		dtrace_actkind_t	act;
		int (*func)(dtrace_hdl_t *, FILE *, void *,
			    const dtrace_probedata_t *,
			    const dtrace_recdesc_t *, uint_t,
			    const void *buf, size_t) = NULL;
		caddr_t			recdata;

		rec = &epd->dtdd_recs[i];
		act = rec->dtrd_action;
		pdat->dtpda_data = recdata = data + rec->dtrd_offset;

		if (act == DTRACEACT_LIBACT) {
			switch (rec->dtrd_arg) {
			case DT_ACT_DENORMALIZE:
				if (dt_normalize(dtp, data, rec) != 0)
					return DTRACE_WORKSTATUS_ERROR;

				continue;
			case DT_ACT_NORMALIZE:
				if (i == epd->dtdd_nrecs - 1)
					return dt_set_errno(dtp, EDT_BADNORMAL);

				if (dt_normalize(dtp, data, rec) != 0)
					return DTRACE_WORKSTATUS_ERROR;

				i++;
				continue;
			case DT_ACT_CLEAR:
				if (dt_clear(dtp, data, rec) != 0)
					return DTRACE_WORKSTATUS_ERROR;

				continue;
			case DT_ACT_TRUNC:
				if (i == epd->dtdd_nrecs - 1)
					return dt_set_errno(dtp, EDT_BADTRUNC);

				if (dt_trunc(dtp, data, rec) != 0)
					return DTRACE_WORKSTATUS_ERROR;

				i++;
				continue;
			case DT_ACT_FTRUNCATE:
				if (fp == NULL)
					continue;

				fflush(fp);
				ftruncate(fileno(fp), 0);
				fseeko(fp, 0, SEEK_SET);

				continue;
			case DT_ACT_SETOPT: {
				caddr_t			opt = recdata;
				caddr_t			val;
				dtrace_recdesc_t	*vrec;

				if (i == epd->dtdd_nrecs - 1)
					return dt_set_errno(dtp, EDT_BADSETOPT);

				vrec = &epd->dtdd_recs[++i];
				if (vrec->dtrd_action != act &&
				    vrec->dtrd_arg != rec->dtrd_arg)
					return dt_set_errno(dtp, EDT_BADSETOPT);

				/*
				 * Two possibilities: either a string was
				 * passed (alignment 1), or a uint32_t was
				 * passed (alignment 4).  The latter indicates
				 * a toggle option (no value).
				 */
				if (vrec->dtrd_alignment == 1)
					val = data + vrec->dtrd_offset;
				else
					val = "1";

				if (dt_setopt(dtp, pdat, opt, val) != 0)
					return DTRACE_WORKSTATUS_ERROR;

				continue;
			}
			default:
				return dt_set_errno(dtp, EDT_ERRABORT);
			}
		}

		if (act == DTRACEACT_COMMIT || act == DTRACEACT_DISCARD) {
			/*
			 * Committing or discarding.  If this is the first
			 * commit/discard we've seen for this speculation,
			 * arrange to drain it until enough CPUs have passed by
			 * that all must have been drained.  Out-of-range IDs
			 * are automatically ignored by the code below, since
			 * they will have no dtsb entries.
			 */

			assert(specid == 0);

			tmpl.dtsb_id = *(uint32_t *)recdata;
			dtsb = dt_htab_lookup(dtp->dt_spec_bufs, &tmpl);

			/*
			 * Speculation exists and is not already being drained.
			 */
			if (dtsb && !dtsb->dtsb_spec.draining) {
				dtsb_draining_t *dtsd;
				dt_bpf_specs_t spec;
				dt_ident_t *idp = dt_dlib_get_map(dtp, "specs");
				int rval;

				rval = dt_bpf_map_lookup(idp->di_id, &tmpl.dtsb_id, &spec);
				if (rval != 0)
					return dt_set_errno(dtp, -rval);

				assert(spec.draining);
				dtsd = dt_zalloc(dtp, sizeof(dtsb_draining_t));
				if (!dtsd)
					return dt_set_errno(dtp, EDT_NOMEM);
				dtsd->dtsd_dtsb = dtsb;
				dtsb->dtsb_committing = (act == DTRACEACT_COMMIT);
				memcpy(&dtsb->dtsb_spec, &spec,
				    sizeof(dt_bpf_specs_t));
				dt_list_append(&dtp->dt_spec_bufs_draining, dtsd);
			}
			continue;
		}

		assert(data_recording);

		rval = (*rfunc)(pdat, rec, arg);

		if (rval == DTRACE_CONSUME_NEXT)
			continue;

		if (rval == DTRACE_CONSUME_ABORT)
			return dt_set_errno(dtp, EDT_DIRABORT);

		if (rval != DTRACE_CONSUME_THIS)
			return dt_set_errno(dtp, EDT_BADRVAL);

		switch (act) {
		case DTRACEACT_STACK: {
			int depth = rec->dtrd_arg;

			if (dt_print_stack(dtp, fp, NULL, recdata,
					   depth, rec->dtrd_size / depth) < 0)
				return -1;
			continue;
		}
		case DTRACEACT_SYM:
			if (dt_print_sym(dtp, fp, NULL, recdata) < 0)
				return -1;
			continue;
		case DTRACEACT_MOD:
			if (dt_print_mod(dtp, fp, NULL, recdata) < 0)
				return -1;
			continue;
		case DTRACEACT_USTACK:
			if (dt_print_ustack(dtp, fp, NULL,
					    recdata, rec->dtrd_arg) < 0)
				return -1;
			continue;
		case DTRACEACT_USYM:
		case DTRACEACT_UADDR:
			if (dt_print_usym(dtp, fp, recdata, act) < 0)
				return -1;
			continue;
		case DTRACEACT_UMOD:
			if (dt_print_umod(dtp, fp, NULL, recdata) < 0)
				return -1;
			continue;
		case DTRACEACT_TRACEMEM: {
			int nrecs;

			nrecs = epd->dtdd_nrecs - i;
			n = dt_print_tracemem(dtp, fp, rec, nrecs, data);
			if (n < 0)
				return -1;

			i += n - 1;
			continue;
		}
		case DTRACEACT_PCAP: {
			int	nrecs;

			nrecs = epd->dtdd_nrecs - i;
			n = dt_print_pcap(dtp, fp, rec, nrecs, data);
			if (n < 0)
				return -1;
			i += n - 1;
			continue;
		}
		case DTRACEACT_PRINTF:
			func = dtrace_fprintf;
			break;
		case DTRACEACT_PRINTA:
			if (rec->dtrd_format != NULL)
				func = dtrace_fprinta;
			else
				func = dt_printa;
			break;
		case DTRACEACT_SYSTEM:
			func = dtrace_system;
			break;
		case DTRACEACT_FREOPEN:
			func = dtrace_freopen;
			break;
		case DTRACEACT_PRINT:
			func = dt_print_type;
			break;
		default:
			break;
		}

		if (func) {
			int	nrecs;

			nrecs = epd->dtdd_nrecs - i;
			n = (*func)(dtp, fp, rec->dtrd_format, pdat,
				    rec, nrecs, data, size);
			if (n < 0)
				return DTRACE_WORKSTATUS_ERROR;
			if (n > 0)
				i += n - 1;

			continue;
		}

		n = dt_print_trace(dtp, fp, rec, recdata, quiet);
		if (n < 0)
			return DTRACE_WORKSTATUS_ERROR;
	}

	/*
	 * Call the record callback with a NULL record to indicate
	 * that we're done processing this EPID.  The return value is ignored in
	 * this case. XXX should we respect at least DTRACE_CONSUME_ABORT?
	 */
	if (data_recording) {
		(*rfunc)(pdat, NULL, arg);

		*last = epid;
	}

	/*
	 * If we're not committing a speculative buffer already, commit any spec
	 * buffers that are marked as committing and draining and have any
	 * content to drain.
	 */
	if (!committing) {
		dtsb_draining_t *dtsd, *next;

		for (dtsd = dt_list_next(&dtp->dt_spec_bufs_draining);
		     dtsd != NULL; dtsd = next) {

			dtsb = dtsd->dtsd_dtsb;
			dtsd->dtsd_read += dt_list_length(&dtsb->dtsb_dsbd_list);

			if (dtsb->dtsb_committing) {
				if ((ret = dt_commit_one_spec(dtp, fp, dtsb,
							      pdat, efunc, rfunc,
							      flow, quiet, peekflags,
							      last, arg)) !=
				    DTRACE_WORKSTATUS_OKAY) {
					dt_spec_buf_data_destroy(dtp, dtsb);
					return ret;
				}
			}

			next = dt_list_next(dtsd);

			if (dtsd->dtsd_read < dtsb->dtsb_spec.written)
				dt_spec_buf_data_destroy(dtp, dtsb);
			else {
				dt_list_delete(&dtp->dt_spec_bufs_draining, dtsd);
				dt_htab_delete(dtp->dt_spec_bufs, dtsb);
				dt_free(dtp, dtsd);
			}
		}
	}

	return DTRACE_WORKSTATUS_OKAY;
}

static dtrace_workstatus_t
dt_consume_one(dtrace_hdl_t *dtp, FILE *fp, char *buf,
	       dtrace_probedata_t *pdat, dtrace_consume_probe_f *efunc,
	       dtrace_consume_rec_f *rfunc, int flow, int quiet, int peekflags,
	       dtrace_epid_t *last, void *arg)
{
	char				*data = buf;
	struct perf_event_header	*hdr;

	hdr = (struct perf_event_header *)data;
	data += sizeof(struct perf_event_header);

	if (hdr->type == PERF_RECORD_SAMPLE) {
		char		*ptr = data;
		uint32_t	size;

		/*
		 * struct {
		 *	struct perf_event_header	header;
		 *	uint32_t			size;
		 *	uint32_t			pad;
		 *	uint32_t			epid;
		 *	uint32_t			specid;
		 *	uint64_t			data[n];
		 * }
		 * and 'data' points to the 'size' member at this point.
		 * (Note that 'n' may be 0.)
		 */
		if (ptr > buf + hdr->size)
			return dt_set_errno(dtp, EDT_DSIZE);

		size = *(uint32_t *)data;
		data += sizeof(size);
		ptr += sizeof(size) + size;
		if (ptr != buf + hdr->size)
			return dt_set_errno(dtp, EDT_DSIZE);

		data += sizeof(uint32_t);		/* skip padding */
		size -= sizeof(uint32_t);

		return dt_consume_one_probe(dtp, fp, data, size, pdat, efunc,
					    rfunc, flow, quiet, peekflags,
					    last, 0, arg);
	} else if (hdr->type == PERF_RECORD_LOST) {
		return DTRACE_WORKSTATUS_OKAY;
	} else
		return DTRACE_WORKSTATUS_ERROR;
}

static inline uint64_t
ring_buffer_read_head(volatile struct perf_event_mmap_page *rb_page)
{
	uint64_t	head = rb_page->data_head;

	asm volatile("" : : : "memory");
	return head;
}

static inline void
ring_buffer_write_tail(volatile struct perf_event_mmap_page *rb_page,
		       uint64_t tail)
{
	asm volatile("" : : : "memory");
	rb_page->data_tail = tail;
}

int
dt_consume_cpu(dtrace_hdl_t *dtp, FILE *fp, dt_peb_t *peb,
	       dtrace_consume_probe_f *efunc, dtrace_consume_rec_f *rfunc,
	       int peekflags, void *arg)
{
	struct perf_event_mmap_page	*rb_page = (void *)peb->base;
	struct perf_event_header	*hdr;
	dtrace_epid_t			last = DTRACE_EPIDNONE;
	char				*base;
	char				*event;
	uint32_t			len;
	uint64_t			head, tail;
	dt_pebset_t			*pebset = dtp->dt_pebset;
	uint64_t			data_size = pebset->data_size;
	int				flow, quiet;
	dtrace_probedata_t		pdat;

	flow = (dtp->dt_options[DTRACEOPT_FLOWINDENT] != DTRACEOPT_UNSET);
	quiet = (dtp->dt_options[DTRACEOPT_QUIET] != DTRACEOPT_UNSET);

	if (dt_check_cpudrops(dtp, peb->cpu, DTRACEDROP_PRINCIPAL) != 0)
		return DTRACE_WORKSTATUS_ERROR;

	/*
	 * Clear the probe data, and fill in data independent fields.
	 *
	 * Initializing more fields here (or anywhere above
	 * dt_consume_one_probe) may require the addition of new fields to
	 * dt_spec_bufs, if you want the original value to be preserved across
	 * speculate/commit.
	 */
	memset(&pdat, 0, sizeof(pdat));
	pdat.dtpda_handle = dtp;
	pdat.dtpda_cpu = peb->cpu;

	/*
	 * Set base to be the start of the buffer data, i.e. we skip the first
	 * page (it contains buffer management data).
	 */
	base = peb->base + pebset->page_size;

	if (peekflags == CONSUME_PEEK || peekflags == CONSUME_PEEK_FINISH)
		head = peb->last_head;
	else {
		head = ring_buffer_read_head(rb_page);
		peb->last_head = head;
	}
	tail = rb_page->data_tail;

	while (tail != head) {
		dtrace_workstatus_t rval = DTRACE_WORKSTATUS_OKAY;

		event = base + tail % data_size;
		hdr = (struct perf_event_header *)event;
		len = hdr->size;

		/*
		 * If the perf event data wraps around the boundary of
		 * the buffer, we make a copy in contiguous memory.
		 */
                if (event + len > peb->endp) {
                  char *dst;
                  uint32_t num;

                  /* Increase the buffer as needed. */
                  if (pebset->tmp_len < len) {
                    pebset->tmp = realloc(pebset->tmp, len);
                    pebset->tmp_len = len;
                  }

                  dst = pebset->tmp;
                  num = peb->endp - event + 1;
                  memcpy(dst, event, num);
                  memcpy(dst + num, base, len - num);

                  event = dst;
                }

                rval = dt_consume_one(dtp, fp, event, &pdat, efunc, rfunc, flow,
                                      quiet, peekflags, &last, arg);
                if (rval == DTRACE_WORKSTATUS_DONE)
                  return DTRACE_WORKSTATUS_OKAY;
                if (rval != DTRACE_WORKSTATUS_OKAY)
                  return rval;

                tail += hdr->size;
	}

	if (peekflags == 0 || peekflags == CONSUME_PEEK_FINISH)
		ring_buffer_write_tail(rb_page, tail);

        return DTRACE_WORKSTATUS_OKAY;
}

typedef struct dt_begin {
	dtrace_consume_probe_f *dtbgn_probefunc;
	dtrace_consume_rec_f *dtbgn_recfunc;
	void *dtbgn_arg;
	dtrace_handle_err_f *dtbgn_errhdlr;
	void *dtbgn_errarg;
	int dtbgn_beginonly;
} dt_begin_t;

static int
dt_consume_begin_probe(const dtrace_probedata_t *data, void *arg)
{
	dt_begin_t		*begin = (dt_begin_t *)arg;
	dtrace_probedesc_t	*pd = data->dtpda_pdesc;
	int			r1 = (strcmp(pd->prv, "dtrace") == 0);
	int			r2 = (strcmp(pd->prb, "BEGIN") == 0);

	if (begin->dtbgn_beginonly) {
		if (!(r1 && r2))
			return DTRACE_CONSUME_NEXT;
	} else {
		if (r1 && r2)
			return DTRACE_CONSUME_NEXT;
	}

	/*
	 * We have a record that we're interested in.  Now call the underlying
	 * probe function...
	 */
	return begin->dtbgn_probefunc(data, begin->dtbgn_arg);
}

static int
dt_consume_begin_record(const dtrace_probedata_t *data,
			const dtrace_recdesc_t *rec, void *arg)
{
	dt_begin_t	*begin = (dt_begin_t *)arg;

	return begin->dtbgn_recfunc(data, rec, begin->dtbgn_arg);
}

static int
dt_consume_begin_error(const dtrace_errdata_t *data, void *arg)
{
	dt_begin_t		*begin = (dt_begin_t *)arg;
	dtrace_probedesc_t	*pd = data->dteda_pdesc;
	int			r1 = (strcmp(pd->prv, "dtrace") == 0);
	int			r2 = (strcmp(pd->prb, "BEGIN") == 0);

	if (begin->dtbgn_beginonly) {
		if (!(r1 && r2))
			return DTRACE_HANDLE_OK;
	} else {
		if (r1 && r2)
			return DTRACE_HANDLE_OK;
	}

	return begin->dtbgn_errhdlr(data, begin->dtbgn_errarg);
}

/*
 * There is this idea that the BEGIN probe should be processed before
 * everything else, and that the END probe should be processed after anything
 * else.
 *
 * In the common case, this is pretty easy to deal with.  However, a situation
 * may arise where the BEGIN enabling and END enabling are on the same CPU, and
 * some enabling in the middle occurred on a different CPU.
 *
 * To deal with this (blech!) we need to consume the BEGIN buffer up until the
 * end of the BEGIN probe, and then set it aside.  We will then process every
 * other CPU, and then we'll return to the BEGIN CPU and process the rest of
 * the data (which will inevitably include the END probe, if any).
 *
 * Making this even more complicated (!) is the library's ERROR enabling.
 * Because this enabling is processed before we even get into the consume call
 * back, any ERROR firing would result in the library's ERROR enabling being
 * processed twice -- once in our first pass (for BEGIN probes), and again in
 * our second pass (for everything but BEGIN probes).
 *
 * To deal with this, we interpose on the ERROR handler to assure that we only
 * process ERROR enablings induced by BEGIN enablings in the first pass, and
 * that we only process ERROR enablings _not_ induced by BEGIN enablings in the
 * second pass.
 */
static dtrace_workstatus_t
dt_consume_begin(dtrace_hdl_t *dtp, FILE *fp, struct epoll_event *events,
		 int cnt, dtrace_consume_probe_f *pf, dtrace_consume_rec_f *rf,
		 void *arg)
{
	dt_peb_t	*bpeb = NULL;
	dt_begin_t	begin;
	processorid_t	cpu = dtp->dt_beganon;
	int		rval, i;

	/*
	 * Ensure we get called only once...
	 */
	dtp->dt_beganon = -1;

	/*
	 * Find the buffer for the CPU on which the BEGIN probe executed).
	 */
	for (i = 0; i < cnt; i++) {
		bpeb = events[i].data.ptr;

		if (bpeb == NULL)
			continue;
		if (bpeb->cpu == cpu)
			break;
	}

	/*
	 * If not found, the BEGIN probe does not have data recording clauses
	 * so we are done here.
	 */
	if (bpeb == NULL || bpeb->cpu != cpu)
		return DTRACE_WORKSTATUS_OKAY;

	/*
	 * The simple case: we are either not stopped, or we are stopped and the
	 * END probe executed on another CPU.  Simply consume this buffer and
	 * return.
	 */
	if (!dtp->dt_stopped || cpu != dtp->dt_endedon)
		return dt_consume_cpu(dtp, fp, bpeb, pf, rf, 0, arg);

	begin.dtbgn_probefunc = pf;
	begin.dtbgn_recfunc = rf;
	begin.dtbgn_arg = arg;
	begin.dtbgn_beginonly = 1;

	/*
	 * We need to interpose on the ERROR handler to be sure that we only
	 * process ERRORs induced by BEGIN.
	 */
	begin.dtbgn_errhdlr = dtp->dt_errhdlr;
	begin.dtbgn_errarg = dtp->dt_errarg;
	dtp->dt_errhdlr = dt_consume_begin_error;
	dtp->dt_errarg = &begin;

	rval = dt_consume_cpu(dtp, fp, bpeb, dt_consume_begin_probe,
			      dt_consume_begin_record, CONSUME_PEEK_START,
			      &begin);

	dtp->dt_errhdlr = begin.dtbgn_errhdlr;
	dtp->dt_errarg = begin.dtbgn_errarg;

	if (rval != 0)
		return rval;

	for (i = 0; i < cnt; i++) {
		dt_peb_t	*peb = events[i].data.ptr;

		if (peb == NULL || peb == bpeb)
			continue;

		rval = dt_consume_cpu(dtp, fp, peb, pf, rf, 0, arg);
		if (rval != 0)
			return rval;
	}

	/*
	 * Okay -- we're done with the other buffers.  Now we want to reconsume
	 * the first buffer -- but this time we're looking for everything _but_
	 * BEGIN.  And of course, in order to only consume those ERRORs _not_
	 * associated with BEGIN, we need to reinstall our ERROR interposition
	 * function...
	 */
	begin.dtbgn_beginonly = 0;

	assert(begin.dtbgn_errhdlr == dtp->dt_errhdlr);
	assert(begin.dtbgn_errarg == dtp->dt_errarg);
	dtp->dt_errhdlr = dt_consume_begin_error;
	dtp->dt_errarg = &begin;

	rval = dt_consume_cpu(dtp, fp, bpeb, dt_consume_begin_probe,
			      dt_consume_begin_record, CONSUME_PEEK_FINISH,
			      &begin);

	dtp->dt_errhdlr = begin.dtbgn_errhdlr;
	dtp->dt_errarg = begin.dtbgn_errarg;

	return rval;
}

static void
dt_consume_proc_exits(dtrace_hdl_t *dtp)
{
	dt_proc_hash_t		*dph = dtp->dt_procs;
	dt_proc_notify_t	*dprn;

	/*
	 * Iterate over any pending notifications that may have accumulated
	 * while we were asleep, and process them.
	 */
	pthread_mutex_lock(&dph->dph_lock);

	while ((dprn = dph->dph_notify) != NULL) {
		if (dtp->dt_prochdlr != NULL) {
			char	*err = dprn->dprn_errmsg;
			pid_t	pid = dprn->dprn_pid;
			int	state = PS_DEAD;

			/*
			 * The dprn_dpr may be NULL if attachment or process
			 * creation has failed, or once the process dies.  Only
			 * get the state of a dprn that is not NULL.
			 */
			if (dprn->dprn_dpr != NULL) {
				pid = dprn->dprn_dpr->dpr_pid;
				dt_proc_lock(dprn->dprn_dpr);
			}

			if (*err == '\0')
				err = NULL;

			if (dprn->dprn_dpr != NULL)
				state = dt_Pstate(dtp, pid);

			if (state < 0 || state == PS_DEAD)
				pid *= -1;

			if (dprn->dprn_dpr != NULL)
				dt_proc_unlock(dprn->dprn_dpr);

			dtp->dt_prochdlr(pid, err, dtp->dt_procarg);
		}

		dph->dph_notify = dprn->dprn_next;
		dt_free(dtp, dprn);
	}

	pthread_mutex_unlock(&dph->dph_lock);
}

int
dt_consume_init(dtrace_hdl_t *dtp)
{
	dtp->dt_spec_bufs = dt_htab_create(dtp, &dt_spec_buf_htab_ops);

	if (!dtp->dt_spec_bufs)
		return dt_set_errno(dtp, EDT_NOMEM);
	return 0;
}

void
dt_consume_fini(dtrace_hdl_t *dtp)
{
	dtsb_draining_t *dtsd;

	while ((dtsd = dt_list_next(&dtp->dt_spec_bufs_draining)) != NULL) {
		dt_list_delete(&dtp->dt_spec_bufs_draining, dtsd);
		dt_free(dtp, dtsd);
	}

	dt_htab_destroy(dtp, dtp->dt_spec_bufs);
}

dtrace_workstatus_t
dtrace_consume(dtrace_hdl_t *dtp, FILE *fp, dtrace_consume_probe_f *pf,
	       dtrace_consume_rec_f *rf, void *arg)
{
	dtrace_optval_t		interval = dtp->dt_options[DTRACEOPT_SWITCHRATE];
	struct epoll_event	events[dtp->dt_conf.num_online_cpus];
	int			drained = 0;
	int			i, cnt;
	dtrace_workstatus_t	rval;

	/* Has tracing started yet? */
	if (!dtp->dt_active)
		return dt_set_errno(dtp, EINVAL);

	if (interval > 0) {
		hrtime_t	now = gethrtime();

		if (dtp->dt_lastswitch != 0) {
			if (now - dtp->dt_lastswitch < interval)
				return DTRACE_WORKSTATUS_OKAY;

			dtp->dt_lastswitch += interval;
		} else
			dtp->dt_lastswitch = now;
	} else
		interval = NANOSEC;			/* 1s */

	/*
	 * Ensure that we have callback functions to use (if none we provided,
	 * we use the default no-op ones).
	 */
	if (pf == NULL)
		pf = (dtrace_consume_probe_f *)dt_nullprobe;

	if (rf == NULL)
		rf = (dtrace_consume_rec_f *)dt_nullrec;

	/*
	 * The epoll_wait() function expects the interval to be expressed in
	 * milliseconds whereas the switch rate is expressed in nanoseconds.
	 * We therefore need to convert the value.
	 */
	interval /= NANOSEC / MILLISEC;
	cnt = epoll_wait(dtp->dt_poll_fd, events, dtp->dt_conf.num_online_cpus,
			 interval);
	if (cnt < 0) {
		dt_set_errno(dtp, errno);
		return DTRACE_WORKSTATUS_ERROR;
	}

	/*
	 * See if there are notifications pending from the proc handling code.
	 * If there are, we process them first.
	 */
	for (i = 0; i < cnt; i++) {
		if (events[i].data.ptr == dtp->dt_procs) {
			eventfd_t	dummy;

			eventfd_read(dtp->dt_proc_fd, &dummy);
			dt_consume_proc_exits(dtp);
			events[i].data.ptr = NULL;
		}
	}

	if (dtrace_aggregate_snap(dtp) == DTRACE_WORKSTATUS_ERROR)
		return DTRACE_WORKSTATUS_ERROR;

	/*
	 * If dtp->dt_beganon is not -1, we did not process the BEGIN probe
	 * data (if any) yet.  We do know (since dtp->dt_active is TRUE) that
	 * the BEGIN probe completed processing and that it therefore recorded
	 * the id of the CPU it executed on in DT_STATE_BEGANON.
	 */
	if (dtp->dt_beganon != -1) {
		rval = dt_consume_begin(dtp, fp, events, cnt, pf, rf, arg);
		if (rval != 0)
			return rval;

		/* Force data retrieval since BEGIN was processed. */
		dtp->dt_lastagg = 0;
		dtp->dt_lastswitch = 0;
	}

	/*
	 * Loop over the buffers that have data available, and process them one
	 * by one.  If tracing has stopped, skip the CPU on which the END probe
	 * executed because we want to process that one last.
	 */
drain:
	for (i = 0; i < cnt; i++) {
		dt_peb_t	*peb = events[i].data.ptr;

		if (peb == NULL)
			continue;
		if (dtp->dt_stopped && peb->cpu == dtp->dt_endedon)
			continue;

		rval = dt_consume_cpu(dtp, fp, peb, pf, rf, 0, arg);
		if (rval != 0)
			return rval;
	}

	/*
	 * If a commit or discard has come in, loop twice, because if it was a
	 * commit the commit probably wasn't the first in the CPU list, and it is
	 * quite likely that an earlier CPU contains some of the speculative
	 * content we want to commit.  Circle round and process it once more to
	 * pick this up.  This means users don't find themselves with committed
	 * speculative content routinely split between one consume loop and the
	 * next.
	 */
	if (!drained && dt_list_next(&dtp->dt_spec_bufs_draining) != NULL) {
		drained = 1;
		goto drain;
	}

	/*
	 * If tracing has not been stopped, we are done here.
	 */
	if (!dtp->dt_stopped)
		return 0;

	/*
	 * Tracing has stopped, so we need to process the buffer for the CPU on
	 * which the END probe executed.
	 */
	for (i = 0; i < cnt; i++) {
		dt_peb_t	*peb = events[i].data.ptr;

		if (peb == NULL)
			continue;
		if (peb->cpu != dtp->dt_endedon)
			continue;

		return dt_consume_cpu(dtp, fp, peb, pf, rf, 0, arg);
	}

	/*
	 * If we get here, the END probe fired without any data being recorded
	 * for it.  That's OK.
	 */
	return DTRACE_WORKSTATUS_OKAY;
}
