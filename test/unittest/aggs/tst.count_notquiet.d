/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Aggregations are not data-recording.
 *
 * SECTION: Aggregations/Aggregations
 */

/* three default actions */
BEGIN { }
BEGIN { }
BEGIN { }

/* one explicit action */
BEGIN { trace("hello world"); }

/* ten non-data-recording actions */
BEGIN { @a = count(); }
BEGIN { @a = count(); }
BEGIN { @a = count(); }
BEGIN { @a = count(); }
BEGIN { @a = count(); }
BEGIN { @a = count(); }
BEGIN { @a = count(); }
BEGIN { @a = count(); }
BEGIN { @a = count(); }
BEGIN { @a = count(); }

/* exit */
BEGIN { exit(0); }
