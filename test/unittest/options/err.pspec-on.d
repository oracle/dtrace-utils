/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * In the presence of "pspec", this ambiguous expression would seem to be
 * missing a predicate or action after what is interpreted as a probe
 * description.
 */

#pragma D option pspec

int*x
