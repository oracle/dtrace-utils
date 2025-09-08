
# inet\_ntoa

Returns a dotted, quad decimal string for a pointer to an IPv4 address.

```
string inet_ntoa(ipaddr_t *)
```

The `inet_ntoa` function takes a pointer to an IPv4 address and returns it as a dotted, quad decimal string. The returned string is allocated out of scratch memory and is therefore valid only during processing of the clause. If insufficient scratch memory is available, `inet_ntoa` doesn't run and an error is generated. See the `inet(3)` manual page for more information.

## How to use inet\_ntoa to return dotted IPv4 address notation for a pointer to an IPv4 address

In the example, an IP address pointer is created in scratch memory and populated so that the `inet_ntoa` function can process it and return a string value.

```
 typedef vmlinux`__be32 ipaddr_t;
 ipaddr_t *ip4a;
 BEGIN
 {
         ip4a = alloca(sizeof(ipaddr_t));
         *ip4a = 0x0100007f;
         printf("%s\n", inet_ntoa(ip4a));
         exit(0);
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

