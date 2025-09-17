
# link\_ntop

Returns a string translation of a hardware address.

```nocopybutton
string link_ntop(int *hardware\_type*, void **addr*)
```

The `link_ntop` function translates a hardware address into a string. The *hardware\_type* can be `ARPHRD_ETHER` or `ARPHRD_INFINIBAND`. The returned string is allocated out of scratch memory, and is therefore valid only during the clause. If insufficient scratch space is available, `link_ntop` doesn't run and an error is generated. The function is the link-level equivalent of `inet_ntop`.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

