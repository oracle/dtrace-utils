#!/usr/bin/gawk -f

BEGIN {
        mypid = -1
}

/^01 [1-9][0-9]*$/ {
        mypid = $NF;
}

{
        if ($NF == mypid)
                $NF = "mypid";
        print;
}
