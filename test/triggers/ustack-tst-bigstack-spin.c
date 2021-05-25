/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

int mycallee(int i) {
	return i ^ 1;
}

int myfunc_30() {
	int i = 1;

	/* call another function to prevent leaf call optimizations */
	i = mycallee(i);

	/* endless loop, profile-n probe will fire here */
	while (i < 3)
		i = 2 * i - 1;

	return i;
}

/* have a very deep stack; ^1 prevents tail calls */
int myfunc_2z() { return myfunc_30() ^ 1; }
int myfunc_2y() { return myfunc_2z() ^ 1; }
int myfunc_2x() { return myfunc_2y() ^ 1; }
int myfunc_2w() { return myfunc_2x() ^ 1; }
int myfunc_2v() { return myfunc_2w() ^ 1; }
int myfunc_2u() { return myfunc_2v() ^ 1; }
int myfunc_2t() { return myfunc_2u() ^ 1; }
int myfunc_2s() { return myfunc_2t() ^ 1; }
int myfunc_2r() { return myfunc_2s() ^ 1; }
int myfunc_2q() { return myfunc_2r() ^ 1; }
int myfunc_2p() { return myfunc_2q() ^ 1; }
int myfunc_2o() { return myfunc_2p() ^ 1; }
int myfunc_2n() { return myfunc_2o() ^ 1; }
int myfunc_2m() { return myfunc_2n() ^ 1; }
int myfunc_2l() { return myfunc_2m() ^ 1; }
int myfunc_2k() { return myfunc_2l() ^ 1; }
int myfunc_2j() { return myfunc_2k() ^ 1; }
int myfunc_2i() { return myfunc_2j() ^ 1; }
int myfunc_2h() { return myfunc_2i() ^ 1; }
int myfunc_2g() { return myfunc_2h() ^ 1; }
int myfunc_2f() { return myfunc_2g() ^ 1; }
int myfunc_2e() { return myfunc_2f() ^ 1; }
int myfunc_2d() { return myfunc_2e() ^ 1; }
int myfunc_2c() { return myfunc_2d() ^ 1; }
int myfunc_2b() { return myfunc_2c() ^ 1; }
int myfunc_2a() { return myfunc_2b() ^ 1; }
int myfunc_2Z() { return myfunc_2a() ^ 1; }
int myfunc_2Y() { return myfunc_2Z() ^ 1; }
int myfunc_2X() { return myfunc_2Y() ^ 1; }
int myfunc_2W() { return myfunc_2X() ^ 1; }
int myfunc_2V() { return myfunc_2W() ^ 1; }
int myfunc_2U() { return myfunc_2V() ^ 1; }
int myfunc_2T() { return myfunc_2U() ^ 1; }
int myfunc_2S() { return myfunc_2T() ^ 1; }
int myfunc_2R() { return myfunc_2S() ^ 1; }
int myfunc_2Q() { return myfunc_2R() ^ 1; }
int myfunc_2P() { return myfunc_2Q() ^ 1; }
int myfunc_2O() { return myfunc_2P() ^ 1; }
int myfunc_2N() { return myfunc_2O() ^ 1; }
int myfunc_2M() { return myfunc_2N() ^ 1; }
int myfunc_2L() { return myfunc_2M() ^ 1; }
int myfunc_2K() { return myfunc_2L() ^ 1; }
int myfunc_2J() { return myfunc_2K() ^ 1; }
int myfunc_2I() { return myfunc_2J() ^ 1; }
int myfunc_2H() { return myfunc_2I() ^ 1; }
int myfunc_2G() { return myfunc_2H() ^ 1; }
int myfunc_2F() { return myfunc_2G() ^ 1; }
int myfunc_2E() { return myfunc_2F() ^ 1; }
int myfunc_2D() { return myfunc_2E() ^ 1; }
int myfunc_2C() { return myfunc_2D() ^ 1; }
int myfunc_2B() { return myfunc_2C() ^ 1; }
int myfunc_2A() { return myfunc_2B() ^ 1; }
int myfunc_29() { return myfunc_2A() ^ 1; }
int myfunc_28() { return myfunc_29() ^ 1; }
int myfunc_27() { return myfunc_28() ^ 1; }
int myfunc_26() { return myfunc_27() ^ 1; }
int myfunc_25() { return myfunc_26() ^ 1; }
int myfunc_24() { return myfunc_25() ^ 1; }
int myfunc_23() { return myfunc_24() ^ 1; }
int myfunc_22() { return myfunc_23() ^ 1; }
int myfunc_21() { return myfunc_22() ^ 1; }
int myfunc_20() { return myfunc_21() ^ 1; }
int myfunc_1z() { return myfunc_20() ^ 1; }
int myfunc_1y() { return myfunc_1z() ^ 1; }
int myfunc_1x() { return myfunc_1y() ^ 1; }
int myfunc_1w() { return myfunc_1x() ^ 1; }
int myfunc_1v() { return myfunc_1w() ^ 1; }
int myfunc_1u() { return myfunc_1v() ^ 1; }
int myfunc_1t() { return myfunc_1u() ^ 1; }
int myfunc_1s() { return myfunc_1t() ^ 1; }
int myfunc_1r() { return myfunc_1s() ^ 1; }
int myfunc_1q() { return myfunc_1r() ^ 1; }
int myfunc_1p() { return myfunc_1q() ^ 1; }
int myfunc_1o() { return myfunc_1p() ^ 1; }
int myfunc_1n() { return myfunc_1o() ^ 1; }
int myfunc_1m() { return myfunc_1n() ^ 1; }
int myfunc_1l() { return myfunc_1m() ^ 1; }
int myfunc_1k() { return myfunc_1l() ^ 1; }
int myfunc_1j() { return myfunc_1k() ^ 1; }
int myfunc_1i() { return myfunc_1j() ^ 1; }
int myfunc_1h() { return myfunc_1i() ^ 1; }
int myfunc_1g() { return myfunc_1h() ^ 1; }
int myfunc_1f() { return myfunc_1g() ^ 1; }
int myfunc_1e() { return myfunc_1f() ^ 1; }
int myfunc_1d() { return myfunc_1e() ^ 1; }
int myfunc_1c() { return myfunc_1d() ^ 1; }
int myfunc_1b() { return myfunc_1c() ^ 1; }
int myfunc_1a() { return myfunc_1b() ^ 1; }
int myfunc_1Z() { return myfunc_1a() ^ 1; }
int myfunc_1Y() { return myfunc_1Z() ^ 1; }
int myfunc_1X() { return myfunc_1Y() ^ 1; }
int myfunc_1W() { return myfunc_1X() ^ 1; }
int myfunc_1V() { return myfunc_1W() ^ 1; }
int myfunc_1U() { return myfunc_1V() ^ 1; }
int myfunc_1T() { return myfunc_1U() ^ 1; }
int myfunc_1S() { return myfunc_1T() ^ 1; }
int myfunc_1R() { return myfunc_1S() ^ 1; }
int myfunc_1Q() { return myfunc_1R() ^ 1; }
int myfunc_1P() { return myfunc_1Q() ^ 1; }
int myfunc_1O() { return myfunc_1P() ^ 1; }
int myfunc_1N() { return myfunc_1O() ^ 1; }
int myfunc_1M() { return myfunc_1N() ^ 1; }
int myfunc_1L() { return myfunc_1M() ^ 1; }
int myfunc_1K() { return myfunc_1L() ^ 1; }
int myfunc_1J() { return myfunc_1K() ^ 1; }
int myfunc_1I() { return myfunc_1J() ^ 1; }
int myfunc_1H() { return myfunc_1I() ^ 1; }
int myfunc_1G() { return myfunc_1H() ^ 1; }
int myfunc_1F() { return myfunc_1G() ^ 1; }
int myfunc_1E() { return myfunc_1F() ^ 1; }
int myfunc_1D() { return myfunc_1E() ^ 1; }
int myfunc_1C() { return myfunc_1D() ^ 1; }
int myfunc_1B() { return myfunc_1C() ^ 1; }
int myfunc_1A() { return myfunc_1B() ^ 1; }
int myfunc_19() { return myfunc_1A() ^ 1; }
int myfunc_18() { return myfunc_19() ^ 1; }
int myfunc_17() { return myfunc_18() ^ 1; }
int myfunc_16() { return myfunc_17() ^ 1; }
int myfunc_15() { return myfunc_16() ^ 1; }
int myfunc_14() { return myfunc_15() ^ 1; }
int myfunc_13() { return myfunc_14() ^ 1; }
int myfunc_12() { return myfunc_13() ^ 1; }
int myfunc_11() { return myfunc_12() ^ 1; }
int myfunc_10() { return myfunc_11() ^ 1; }
int myfunc_0z() { return myfunc_10() ^ 1; }
int myfunc_0y() { return myfunc_0z() ^ 1; }
int myfunc_0x() { return myfunc_0y() ^ 1; }
int myfunc_0w() { return myfunc_0x() ^ 1; }
int myfunc_0v() { return myfunc_0w() ^ 1; }
int myfunc_0u() { return myfunc_0v() ^ 1; }
int myfunc_0t() { return myfunc_0u() ^ 1; }
int myfunc_0s() { return myfunc_0t() ^ 1; }
int myfunc_0r() { return myfunc_0s() ^ 1; }
int myfunc_0q() { return myfunc_0r() ^ 1; }
int myfunc_0p() { return myfunc_0q() ^ 1; }
int myfunc_0o() { return myfunc_0p() ^ 1; }
int myfunc_0n() { return myfunc_0o() ^ 1; }
int myfunc_0m() { return myfunc_0n() ^ 1; }
int myfunc_0l() { return myfunc_0m() ^ 1; }
int myfunc_0k() { return myfunc_0l() ^ 1; }
int myfunc_0j() { return myfunc_0k() ^ 1; }
int myfunc_0i() { return myfunc_0j() ^ 1; }
int myfunc_0h() { return myfunc_0i() ^ 1; }
int myfunc_0g() { return myfunc_0h() ^ 1; }
int myfunc_0f() { return myfunc_0g() ^ 1; }
int myfunc_0e() { return myfunc_0f() ^ 1; }
int myfunc_0d() { return myfunc_0e() ^ 1; }
int myfunc_0c() { return myfunc_0d() ^ 1; }
int myfunc_0b() { return myfunc_0c() ^ 1; }
int myfunc_0a() { return myfunc_0b() ^ 1; }
int myfunc_0Z() { return myfunc_0a() ^ 1; }
int myfunc_0Y() { return myfunc_0Z() ^ 1; }
int myfunc_0X() { return myfunc_0Y() ^ 1; }
int myfunc_0W() { return myfunc_0X() ^ 1; }
int myfunc_0V() { return myfunc_0W() ^ 1; }
int myfunc_0U() { return myfunc_0V() ^ 1; }
int myfunc_0T() { return myfunc_0U() ^ 1; }
int myfunc_0S() { return myfunc_0T() ^ 1; }
int myfunc_0R() { return myfunc_0S() ^ 1; }
int myfunc_0Q() { return myfunc_0R() ^ 1; }
int myfunc_0P() { return myfunc_0Q() ^ 1; }
int myfunc_0O() { return myfunc_0P() ^ 1; }
int myfunc_0N() { return myfunc_0O() ^ 1; }
int myfunc_0M() { return myfunc_0N() ^ 1; }
int myfunc_0L() { return myfunc_0M() ^ 1; }
int myfunc_0K() { return myfunc_0L() ^ 1; }
int myfunc_0J() { return myfunc_0K() ^ 1; }
int myfunc_0I() { return myfunc_0J() ^ 1; }
int myfunc_0H() { return myfunc_0I() ^ 1; }
int myfunc_0G() { return myfunc_0H() ^ 1; }
int myfunc_0F() { return myfunc_0G() ^ 1; }
int myfunc_0E() { return myfunc_0F() ^ 1; }
int myfunc_0D() { return myfunc_0E() ^ 1; }
int myfunc_0C() { return myfunc_0D() ^ 1; }
int myfunc_0B() { return myfunc_0C() ^ 1; }
int myfunc_0A() { return myfunc_0B() ^ 1; }
int myfunc_09() { return myfunc_0A() ^ 1; }
int myfunc_08() { return myfunc_09() ^ 1; }
int myfunc_07() { return myfunc_08() ^ 1; }
int myfunc_06() { return myfunc_07() ^ 1; }
int myfunc_05() { return myfunc_06() ^ 1; }
int myfunc_04() { return myfunc_05() ^ 1; }
int myfunc_03() { return myfunc_04() ^ 1; }
int myfunc_02() { return myfunc_03() ^ 1; }
int myfunc_01() { return myfunc_02() ^ 1; }
int myfunc_00() { return myfunc_01() ^ 1; }

int main(int c, char **v) {
	return myfunc_00() ^ 1;
}
