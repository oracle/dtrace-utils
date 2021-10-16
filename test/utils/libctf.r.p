#!/usr/bin/gawk -f
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
/Duplicate member or variable name\.$/ { sub(/\.$/, ""); }
/Member name not found\.$/ { sub(/\.$/, ""); }
/enum union pirate:/ { sub(/enum union pirate/, "enum struct pirate"); }
{ print; }
