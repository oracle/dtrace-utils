7
iregs 8 gives 0
7
iregs 7 gives 0
7
iregs 6 gives 0
iregs 5 gives 1
iregs 4 gives 1
iregs 3 gives 1
-- @@stderr --
dtrace: invalid probe specifier BEGIN {
        a = b = c = d = e = f = g = h = i = j = k = l = 1;
        trace(a + b * (c + d * (e + f * (g + h * (i + j * (k + l))))));
        exit(0);
    }: Insufficient registers to generate code
dtrace: invalid probe specifier BEGIN {
        a = b = c = d = e = f = g = h = i = j = k = l = 1;
        trace(a + b * (c + d * (e + f * (g + h * (i + j * (k + l))))));
        exit(0);
    }: Insufficient registers to generate code
dtrace: invalid probe specifier BEGIN {
        a = b = c = d = e = f = g = h = i = j = k = l = 1;
        trace(a + b * (c + d * (e + f * (g + h * (i + j * (k + l))))));
        exit(0);
    }: Insufficient registers to generate code
