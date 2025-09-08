
# inet\_ntoa6

Returns an RFC 1884 convention 2 string for a pointer to an IPv6 address.

```
string inet_ntoa6(in6_addr_t **addr*)
```

The `inet_ntoa6` function takes a pointer *addr* to an IPv6 address and returns it as an RFC 1884 convention 2 string, with lowercase hexadecimal digits. The returned string is allocated out of scratch memory and is therefore valid only during the clause. If insufficient scratch memory is available, `inet_ntoa6` doesn't run and an error is generated.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

