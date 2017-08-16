/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	@["j-church"] = lquantize(1, 0, 10, 1, 100);
	@["j-church"] = lquantize(1, 0, 10, 1, -99);
	@["j-church"] = lquantize(1, 0, 10, 1, -1);
	val = 123;
}

BEGIN
{
	@["k-ingleside"] = lquantize(1, 0, 10, 1, -val);
}

BEGIN
{
	@["l-taraval"] = lquantize(0, 0, 10, 1, -val);
	@["l-taraval"] = lquantize(-1, 0, 10, 1, -val);
	@["l-taraval"] = lquantize(1, 0, 10, 1, val);
	@["l-taraval"] = lquantize(1, 0, 10, 1, val);
}

BEGIN
{
	@["m-oceanview"] = lquantize(1, 0, 10, 1, (1 << 63) - 1);
	@["m-oceanview"] = lquantize(1, 0, 10, 1);
	@["m-oceanview"] = lquantize(2, 0, 10, 1, (1 << 63) - 1);
	@["m-oceanview"] = lquantize(8, 0, 10, 1, 400000);
}

BEGIN
{
	@["n-judah"] = lquantize(1, 0, 10, 1, val);
	@["n-judah"] = lquantize(2, 0, 10, 1, val);
	@["n-judah"] = lquantize(2, 0, 10, 1, val);
	@["n-judah"] = lquantize(2, 0, 10, 1);
}

BEGIN
{
	this->i = 1;
	this->val = (1 << 63) - 1;

	@["f-market"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["f-market"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["f-market"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["f-market"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["f-market"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["f-market"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["f-market"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;
}

BEGIN
{
	this->i = 1;

	/*
	 * We want to test the ability to sort very large quantizations
	 * that differ by a small amount.  Ideally, they would differ only
	 * by 1 -- but that is smaller than the precision of long doubles of
	 * this magnitude on x86.  To assure that the same test works on x86
	 * just as it does on SPARC, we pick a value that is just larger than
	 * the precision at this magnitude.  It should go without saying that
	 * this robustness on new ISAs very much depends on the precision
	 * of the long double representation.
	 */
	this->val = (1 << 63) - 7;

	@["s-castro"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["s-castro"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["s-castro"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["s-castro"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["s-castro"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["s-castro"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;

	@["s-castro"] = lquantize(this->i, 0, 10, 1, this->val);
	this->i++;
	this->val = ((1 << 63) - 1) / this->i;
}

BEGIN
{
	exit(0);
}
