
# inet\_ntop

Returns an RFC 1884 convention 2 string for a pointer to an IPv6 address.

```
string inet_ntop(int af, void **addr*)
```

The `inet_ntop` function takes a pointer *addr* to an IP address and returns a string version that depends on the provided address family, either AF\_INET, or AF\_INET6. The returned string is allocated out of scratch memory and is therefore valid only during the clause. If insufficient scratch memory is available, `inet_ntop` doesn't run and an error is generated.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

