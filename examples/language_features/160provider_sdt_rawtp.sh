#!/bin/bash

#  SYNOPSIS
#    ./160provider_sdt_rawtp.sh
#
#  DESCRIPTION
#    Statically Defined Tracing (SDT) refers to probes placed
#    in the kernel source code by kernel developers to support
#    tracing in stable, semantically meaningful terms.  In the
#    case of Linux, this means tracepoints.

# You can see these tracepoints via the sdt provider.
sudo /usr/sbin/dtrace -lP sdt

# You can see the exact same probes via the rawtp provider.
sudo /usr/sbin/dtrace -lP rawtp

# The difference is that the rawtp provider gives DTrace users
# access to the raw tracepoints exposed by the kernel tracing
# system, including access to the untranslated arguments of the
# associated tracepoint events.  Regular tracepoints often expose
# translated arguments based on the raw arguments that were passed
# in the tracepoint call.

# For example, the DTrace sched provider implements the "off-cpu"
# probe by using "rawtp:sched::sched_switch" and the "wakeup" probe
# by using "rawtp:sched::sched_wakeup".  We can see how the typed
# arguments for these probes depend on whether we are using the
# sdt or the rawtp provider:

sudo /usr/sbin/dtrace -lvn :sched::sched_switch | grep args
sudo /usr/sbin/dtrace -lvn :sched::sched_wakeup | grep args

# Here, the module "sched" is named, but the provider name,
# expected before the first colon ":", is left blank as a
# wildcard to pick up both providers with these probes.
