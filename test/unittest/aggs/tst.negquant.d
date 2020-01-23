/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

BEGIN
{
	@["j-church"] = quantize(1, 100);
	@["j-church"] = quantize(1, -99);
	@["j-church"] = quantize(1, -1);
	val = 123;
}

BEGIN
{
	@["k-ingleside"] = quantize(1, -val);
}

BEGIN
{
	@["l-taraval"] = quantize(0, -val);
	@["l-taraval"] = quantize(-1, -val);
	@["l-taraval"] = quantize(1, val);
	@["l-taraval"] = quantize(1, val);
}

BEGIN
{
	@["m-oceanview"] = quantize(1, (1 << 63) - 1);
	@["m-oceanview"] = quantize(1);
	@["m-oceanview"] = quantize(2, (1 << 63) - 1);
	@["m-oceanview"] = quantize(8, 400000);
}

BEGIN
{
	@["n-judah"] = quantize(1, val);
	@["n-judah"] = quantize(2, val);
	@["n-judah"] = quantize(2, val);
	@["n-judah"] = quantize(2);
}

BEGIN
{
	this->i = 1;
	this->val = (1 << 63) - 1;

	@["f-market"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= 1;

	@["f-market"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= 1;

	@["f-market"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= 1;

	@["f-market"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= 1;
}

BEGIN
{
	this->i = 1;
	this->val = (1 << 63) - 4;

	@["s-castro"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= 1;

	@["s-castro"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= 1;

	@["s-castro"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= 1;

	@["s-castro"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= 1;
}

BEGIN
{
	exit(0);
}
