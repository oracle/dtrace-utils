/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

int mycallee(int i) {
	return i ^ 1;
}

int myfunc_z() {
	int i = 1;

	/* call another function to prevent leaf call optimizations */
	i = mycallee(i);

	/* endless loop, profile-n probe will fire here */
	while (i < 3)
		i = 2 * i - 1;

	return i;
}

/* have a relatively deep stack; ^1 prevents tail calls */
int myfunc_y() { return myfunc_z() ^ 1; }
int myfunc_x() { return myfunc_y() ^ 1; }
int myfunc_w() { return myfunc_x() ^ 1; }
int myfunc_v() { return myfunc_w() ^ 1; }
int myfunc_u() { return myfunc_v() ^ 1; }
int myfunc_t() { return myfunc_u() ^ 1; }
int myfunc_s() { return myfunc_t() ^ 1; }
int myfunc_r() { return myfunc_s() ^ 1; }
int myfunc_q() { return myfunc_r() ^ 1; }
int myfunc_p() { return myfunc_q() ^ 1; }
int myfunc_o() { return myfunc_p() ^ 1; }
int myfunc_n() { return myfunc_o() ^ 1; }
int myfunc_m() { return myfunc_n() ^ 1; }
int myfunc_l() { return myfunc_m() ^ 1; }
int myfunc_k() { return myfunc_l() ^ 1; }
int myfunc_j() { return myfunc_k() ^ 1; }
int myfunc_i() { return myfunc_j() ^ 1; }
int myfunc_h() { return myfunc_i() ^ 1; }
int myfunc_g() { return myfunc_h() ^ 1; }
int myfunc_f() { return myfunc_g() ^ 1; }
int myfunc_e() { return myfunc_f() ^ 1; }
int myfunc_d() { return myfunc_e() ^ 1; }
int myfunc_c() { return myfunc_d() ^ 1; }
int myfunc_b() { return myfunc_c() ^ 1; }
int myfunc_a() { return myfunc_b() ^ 1; }
int myfunc_Z() { return myfunc_a() ^ 1; }
int myfunc_Y() { return myfunc_Z() ^ 1; }
int myfunc_X() { return myfunc_Y() ^ 1; }
int myfunc_W() { return myfunc_X() ^ 1; }
int myfunc_V() { return myfunc_W() ^ 1; }
int myfunc_U() { return myfunc_V() ^ 1; }
int myfunc_T() { return myfunc_U() ^ 1; }
int myfunc_S() { return myfunc_T() ^ 1; }
int myfunc_R() { return myfunc_S() ^ 1; }
int myfunc_Q() { return myfunc_R() ^ 1; }
int myfunc_P() { return myfunc_Q() ^ 1; }
int myfunc_O() { return myfunc_P() ^ 1; }
int myfunc_N() { return myfunc_O() ^ 1; }
int myfunc_M() { return myfunc_N() ^ 1; }
int myfunc_L() { return myfunc_M() ^ 1; }
int myfunc_K() { return myfunc_L() ^ 1; }
int myfunc_J() { return myfunc_K() ^ 1; }
int myfunc_I() { return myfunc_J() ^ 1; }
int myfunc_H() { return myfunc_I() ^ 1; }
int myfunc_G() { return myfunc_H() ^ 1; }
int myfunc_F() { return myfunc_G() ^ 1; }
int myfunc_E() { return myfunc_F() ^ 1; }
int myfunc_D() { return myfunc_E() ^ 1; }
int myfunc_C() { return myfunc_D() ^ 1; }
int myfunc_B() { return myfunc_C() ^ 1; }
int myfunc_A() { return myfunc_B() ^ 1; }
int myfunc_9() { return myfunc_A() ^ 1; }
int myfunc_8() { return myfunc_9() ^ 1; }
int myfunc_7() { return myfunc_8() ^ 1; }
int myfunc_6() { return myfunc_7() ^ 1; }
int myfunc_5() { return myfunc_6() ^ 1; }
int myfunc_4() { return myfunc_5() ^ 1; }
int myfunc_3() { return myfunc_4() ^ 1; }
int myfunc_2() { return myfunc_3() ^ 1; }
int myfunc_1() { return myfunc_2() ^ 1; }
int myfunc_0() { return myfunc_1() ^ 1; }

int main(int c, char **v) {
	return myfunc_0() ^ 1;
}
