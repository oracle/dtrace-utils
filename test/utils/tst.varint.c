#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dt_varint.h>

void
check(uint64_t val, int exp)
{
	char		s[VARINT_MAX_BYTES];
	const char	*p;
	int		rc, len;
	uint64_t	dval;

	rc = dt_int2vint(val, s);
	if (rc != exp) {
		printf("Length wrong for %lu: %d vs %d\n", val, rc, exp);
		exit(1);
	}
	len = dt_vint_size(val);
	if (len != exp) {
		printf("Size wrong for %lu: %d vs %d\n", val, len, exp);
		exit(1);
	}
	p = dt_vint_skip(s);
	if (!p) {
		printf("Skip wrong for %lu: %d vs %d\n", val, 0, exp);
		exit(1);
	} else if ((p - s) != exp) {
		printf("Skip wrong for %lu: %ld vs %d\n", val, p - s, exp);
		exit(1);
	}
	dval = dt_vint2int(s);
	if (dval != val) {
		printf("Value decode error (%d byte prefix): %lx vs %lx\n",
		       rc, dval, val);
		exit(1);
	}
}

void
check_range(uint64_t lo, uint64_t hi, int len)
{
	uint64_t	val;

	/* taste test!  Here are two styles to choose from: */
#if 0
	for (val = lo - 10000; val < lo; val++)
		check(val, len-1);
	for (val = lo; val < lo + 10000; val++)
		check(val, len);
	for (val = hi - 10000; val <= hi; val++)
		check(val, len);
	for (val = hi + 1; val < hi + 10000; val++)
		check(val, len + 1);
#else
	for (val = lo - 10000; val < lo + 10000; val++)
		check(val, val < lo ? len-1 : len);
	for (val = hi - 10000; val < hi + 10000; val++)
		check(val, val <= hi ? len : len + 1);
#endif
}

int
main(void)
{
	uint64_t	val;

	/* First range: we go through all 16-bit values. */
	for (val = 0; val <= VARINT_1_MAX; val++)
		check(val, 1);
	for (val = VARINT_1_MAX + 1; val <= VARINT_2_MAX; val++)
		check(val, 2);
	for (val = VARINT_2_MAX + 1; val < 0xffff; val++)
		check(val, 3);

	/* For higher ranges, verify the low and high boundary ranges. */
	check_range(VARINT_3_MIN, VARINT_3_MAX, 3);
	check_range(VARINT_4_MIN, VARINT_4_MAX, 4);
	check_range(VARINT_5_MIN, VARINT_5_MAX, 5);
	check_range(VARINT_6_MIN, VARINT_6_MAX, 6);
	check_range(VARINT_7_MIN, VARINT_7_MAX, 7);
	check_range(VARINT_8_MIN, VARINT_8_MAX, 8);

	/* Verify the final range. */
	for (val = VARINT_9_MIN - 10000; val < VARINT_9_MIN; val++)
		check(val, 8);
	for (val = VARINT_9_MIN; val < VARINT_9_MIN + 10000; val++)
		check(val, 9);
	for (val = VARINT_9_MAX - 10000; val < VARINT_9_MAX; val++) {
		check(val, 9);
	}
	check(VARINT_9_MAX, 9);

	return 0;
}

#include "dt_varint.c"
