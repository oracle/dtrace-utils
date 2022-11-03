#!/usr/bin/awk -f

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
/^ *OP2 /      && /ALLOCA/  { print $1, $2, "ALLOCA"; next }
/^ *OP2 /                   { print $1, $2          ; next }
/^ *VARIABLE / && / myvar_/ &&
                  /ALLOCA/  { print $1, $3, "ALLOCA"; next }
/^ *VARIABLE / && / myvar_/ { print $1, $3          ; next }
/^ *FUNC /     && /ALLOCA/  { print $1, $2, "ALLOCA"; next }
/^ *FUNC /                  { print $1, $2          ; next }
/^ *INT /      && /ALLOCA/  { print $1, $2, "ALLOCA"; next }
/^ *INT /                   { print $1, $2          ; next }
/^ *STRING /   && /ALLOCA/  { print $1, $2, "ALLOCA"; next }
/^ *STRING /                { print $1, $2          ; next }
