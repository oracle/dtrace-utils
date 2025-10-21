# UDP Provider

The `udp` provider makes available a probe at UDP send operations in the system and a probe at receive.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## udp Probes <a id="dt_ref_udpprobes_prov">

`udp` provides a probe for UDP sends and one for UDP receives.
The module name is always `vmlinux` and the function name is empty.

## udp Probe Arguments <a id="dt_ref_udpargs_prov">

The following table lists the argument types for `udp` probes.

| probe     | `args[0]`     | `args[1]`    | `args[2]`    | `args[3]`      | `args[4]`     |
| :---      | :---          | :---         | :---         | :---           | :---          |
| `send`    | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `udpsinfo_t *` | `udpinfo_t *` |
| `receive` | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `udpsinfo_t *` | `udpinfo_t *` |

The `pktinfo_t`, `csinfo_t`, and `ipinfo_t` structures are described in [IP Provider](dtrace_providers_ip.md).

### udpsinfo\_t <a id="dt_ref_udpudps_prov">

The `udpsinfo_t` structure contains stable UDP details from `udp_t`.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/udp.d`.
The definition of `udpsinfo_t` is as follows:

```nocopybutton
typedef struct udpsinfo {
        uintptr_t udps_addr;
        uint16_t udps_lport;   /* local port */
        uint16_t udps_rport;   /* remote port */
        string udps_laddr;     /* local address, as a string */
        string udps_raddr;     /* remote address, as a string */
} udpsinfo_t;
```

**Note:**

DTrace translates the members of `udpsinfo_t` from a `struct udp_sock *`.

### udpinfo\_t <a id="dt_ref_udpudp_prov">

The `udpinfo_t` structure contains the UDP header fields.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/udp.d`.
The definition of `udpinfo_t` is as follows:

```nocopybutton
typedef struct udpinfo {
        uint16_t udp_sport;     /* source port */
        uint16_t udp_dport;     /* destination port */
        uint16_t udp_length;    /* total length */
        uint16_t udp_checksum;  /* headers + data checksum */
        struct udphdr *udp_hdr; /* raw UDP header */
} udpinfo_t;
```

**Note:**

DTrace translates the members of `udpinfo_t` from a `struct udphdr *`.

## udp Stability <a id="dt_ref_udpstab_prov">

The `udp` provider uses DTrace's stability mechanism to describe its stabilities. These stability values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | ISA              |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Evolving       | Evolving       | ISA              |
| Arguments | Evolving       | Evolving       | ISA              |
