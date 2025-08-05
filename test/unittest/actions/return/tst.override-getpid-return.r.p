#!/usr/bin/gawk -f

BEGIN {
        mypid = -1
}

/^00 pid is [1-9][0-9]*$/ {
        mypid = $NF;
}

{
        if ($NF == mypid)
                $NF = "mypid";
        print;
}
