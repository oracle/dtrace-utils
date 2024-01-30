#!/usr/bin/gawk -f

{
    # read the first lines (func, mod, sym)
    myfunc = $1; getline;
    mymod  = $1; getline;
    mysym  = $1; getline; getline;

    # read the stack frames
    n = 0;
    while (NF != 0) {
        n++;
        stack[n] = $1;
        getline;
    }
    getline;

    # read the first keys of the agg output
    nerr = 0;
    if ($1 != "97") { nerr++; printf("key #1: expect 97 got %s\n", $1); }
    if ($2 != myfunc) { nerr++; printf("key #2: expect %s got %s\n", myfunc, $2); }
    if ($3 != mymod ) { nerr++; printf("key #3: expect %s got %s\n", mymod , $3); }
    if ($4 != mysym ) { nerr++; printf("key #4: expect %s got %s\n", mysym , $4); }

    # read the stack frames in the agg output
    for (i = 1; i <= n; i++) {
        getline;
        if ($1 != stack[i]) { nerr++; printf("stack frame #%d: expect %s got %s\n", i, stack[i], $1); }
    }

    # read the last key and the value of the agg output
    getline;
    if ($1 != "4") { nerr++; printf("last key: expect 4 got %s\n", $1); }
    if ($2 != "1234") { nerr++; printf("value: expect 1234 got %s\n", $2); }
    if (nerr == 0) { printf("success\n"); exit(0); }
    else           { printf("FAILURE\n"); exit(1); }
}
