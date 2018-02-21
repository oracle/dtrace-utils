#!/bin/sed -f
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
/^hash = /s,= [0-9][0-9]*$,= 0,
/^len = /s,= [1-9][0-9]*$,= 1,
/^name = /s,\"[[:print:]]\"$,"printable-pathname",
