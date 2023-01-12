#!/usr/bin/gawk -f

BEGIN { number = low = high = -1; }

/number/ { number = $2 }
/low/ { low = $2 }
/high/ { high = $2 }

END {
        if (number == -1 || low == -1 || high == -1) {
                printf("missing data\n");
                exit(1);
        }
        if (number < 20 ) {
                printf("too few data\n");
                exit(1);
        }
        if (low != 0 ) {
                printf("some data too low\n");
                exit(1);
        }
        if (high > number / 10 ) {
                printf("too much data too high\n");
                exit(1);
        }
        printf("success\n");
        exit(0);
}
