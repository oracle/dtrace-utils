/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D depends_on module vmlinux
#pragma D depends_on provider lockstat

inline int RW_WRITER = 0x00;
#pragma D binding "1.6.4" RW_WRITER
inline int RW_READER = 0x01;
#pragma D binding "1.6.4" RW_READER
