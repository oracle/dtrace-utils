/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option quiet

BEGIN
{
	skb = (struct sk_buff *)alloca(sizeof (struct sk_buff));
	skb->len = 123;
	/* check for register leaks by doing a lot of print()s */
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	print(skb);
	exit(0);
}
