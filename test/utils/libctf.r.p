#!/usr/bin/gawk -f
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Possible 'enumerator' label.
/Duplicate member or variable name/ { sub(/Duplicate member/, "&, enumerator,"); }

# Possible trailing '.'.
/Duplicate member, enumerator, or variable name\.$/ { sub(/\.$/, ""); }
/Member name not found\.$/ { sub(/\.$/, ""); }

# Could be union or struct.
/enum union pirate:/ { sub(/enum union pirate/, "enum struct pirate"); }

# Older or newer libctf diags.
/\[D_DECL_IDRED\] line [1-9][0-9]*: identifier redeclared: [a-zA-Z]*$/ {
	sub(/\[D_DECL_IDRED\] line [1-9][0-9]*: .*$/, "expected error");
}
/\[D_UNKNOWN\] line [1-9][0-9]*: failed to define enumerator '[a-zA-Z]*': Duplicate member, enumerator, or variable name$/ {
	sub(/\[D_UNKNOWN\] line [1-9][0-9]*: .*$/, "expected error");
}

# Print.
{ print; }
