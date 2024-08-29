#!/usr/bin/gawk -f

# set flag to look for the clause
BEGIN { read_clause = 0 }

# we found the beginning of the clause
/^ *PDESC syscall::write:entry/ {
    read_clause = 1;
    getline;
    getline;
    next;
}

# if not in the clause, keep looking
read_clause == 0 { next }

# if we were reading the clause but get to "Parse tree", we are done
/^Parse tree \(Pass 3\):$/ { exit(0); }

# dump interesting stuff from the clause
/^ *OP2 /      && /DPTR/    { print $1, $2, "DPTR"; next }
/^ *OP2 /                   { print $1, $2        ; next }
/^ *VARIABLE / && / myvar_/ &&
                  /DPTR/    { print $1, $3, "DPTR"; next }
/^ *VARIABLE / && / myvar_/ { print $1, $3        ; next }
/^ *FUNC /     && /DPTR/    { print $1, $2, "DPTR"; next }
/^ *FUNC /                  { print $1, $2        ; next }
/^ *INT /      && /DPTR/    { print $1, $2, "DPTR"; next }
/^ *INT /                   { print $1, $2        ; next }
/^ *STRING /   && /DPTR/    { print $1, $2, "DPTR"; next }
/^ *STRING /                { print $1, $2        ; next }
