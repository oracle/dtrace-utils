run without dtrace
USDT probe is not enabled

USDT probes found:
NNN test_provNNN test main go

run with dtrace but not the USDT probe
USDT probe is not enabled
                   FUNCTION:NAME
                          :BEGIN BEGIN probe fired



run with dtrace and with the USDT probe
USDT probe is enabled

-- @@stderr --
dtrace: description 'BEGIN
                                ' matched 1 probe
dtrace: description 'test_prov$target:::go
                                ' matched 1 probe
