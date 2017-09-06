/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 */

#pragma D depends_on module vmlinux
#pragma	D depends_on provider lockstat

inline int RW_WRITER =	0x00;
#pragma D binding "1.5" RW_WRITER
inline int RW_READER =	0x01;
#pragma D binding "1.5" RW_READER
