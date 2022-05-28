/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -v */

/*
 * ASSERTION:
 * The D inline translation mechanism can be used to facilitate stable
 * translations.
 *
 * SECTION: Translators/ Translator Declarations
 * SECTION: Translators/ Translate Operator
 * SECTION: Translators/Stable Translations
 *
 * NOTES: Uncomment the pragma that explicitly resets the attributes of
 * myinfo identifier to Stable/Stable/Common from Private/Private/Unknown.
 * Run the program with and without the comments as:
 * /usr/sbin/dtrace -vs man.TestTransStability.d
 */

#pragma D option quiet

inline lwpsinfo_t *myinfo = xlate < lwpsinfo_t *> (curthread);

#pragma D attributes Stable/Stable/Common myinfo

BEGIN
{
	trace(myinfo->pr_flag);
	exit(0);
}

ERROR
{
	exit(1);
}
