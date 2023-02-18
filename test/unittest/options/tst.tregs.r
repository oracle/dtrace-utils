tregs 3 gives 1
tregs 5 gives 1

        1        2        3        4        5        6        7                1
tregs 9 gives 0

        1        2        3        4        5        6        7                1
tregs 11 gives 0
-- @@stderr --
dtrace: invalid probe specifier BEGIN { @[1, 2, 3, 4, 5, 6, 7] = count(); exit(0) }: Insufficient tuple registers to generate code
dtrace: invalid probe specifier BEGIN { @[1, 2, 3, 4, 5, 6, 7] = count(); exit(0) }: Insufficient tuple registers to generate code
