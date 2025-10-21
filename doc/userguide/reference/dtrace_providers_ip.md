# IP Provider

The `ip` provider makes available a probe at IP send operations in the system and a probe at receive.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## ip Probes <a id="dt_ref_ipprobes_prov">

The `ip` provider provides one probe for IP sends and another for IP receives.
The module name is always `vmlinux` and the function name is empty.

## ip Probe Arguments <a id="dt_ref_ipargs_prov">

The following table lists the argument types for `ip` probes.

| probe     | `args[0]`     | `args[1]`    | `args[2]`    | `args[3]`    | `args[4]`      | `args[5]`       |
| :---      | :---          | :---         | :---         | :---         | :---           | :---            |
| `send`    | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `ifinfo_t *` | `ipv4info_t *` | `ipv6sinfo_t *` |
| `receive` | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `ifinfo_t *` | `ipv4info_t *` | `ipv6sinfo_t *` |

### pktinfo\_t <a id="dt_ref_ippkt_prov">

The `pktinfo_t` structure is an abstraction that describes a packet.
Detailed information about this data structure can be found in
`/usr/lib64/dtrace/*version*/ip.d` or
`/usr/lib64/dtrace/*version*/net.d`, depending on `dtrace` version.
The definition of `pktinfo_t` is as follows:

```nocopybutton
typedef struct pktinfo {
        uintptr_t pkt_addr;
} pktinfo_t;
```

**Note:**

DTrace translates the members of `pktinfo_t` from the `struct sk_buff *`.

### csinfo\_t <a id="dt_ref_ipcs_prov">

The `csinfo_t` structure is an abstraction that describes connection state.
Detailed information about this data structure can be found in
`/usr/lib64/dtrace/*version*/ip.d` or
`/usr/lib64/dtrace/*version*/net.d`, depending on `dtrace` version.
The definition of `csinfo_t` is as follows:

```nocopybutton
typedef struct csinfo {
	uintptr_t	cs_addr;
	uint64_t	cs_cid;
} csinfo_t;
```

**Note:**

DTrace translates the members of `csinfo_t` from the `struct sock *`.

### ipinfo\_t <a id="dt_ref_ipip_prov">

The `ipinfo_t` structure contains common IP info for both IPv4 and IPv6.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/ip.d`.
The definition of `ipinfo_t` is as follows:

```nocopybutton
typedef struct ipinfo {
        uint8_t ip_ver;         /* IP version (4, 6) */
        uint32_t ip_plength;    /* payload length */
        string ip_saddr;        /* source address */
        string ip_daddr;        /* destination address */
} ipinfo_t;
```

**Note:**

DTrace translates the members of `ipinfo_t` from, variously,
`struct iphdr *`, `struct ipv6hdr *`, or `void_ip_t *`.

### ifinfo\_t <a id="dt_ref_ipif_prov">

The `ifinfo_t` structure contains network interface info.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/ip.d`.
The definition of `ifinfo_t` is as follows:

```nocopybutton
typedef struct ifinfo {
        string if_name;            /* interface name */
        int8_t if_local;           /* is delivered locally */
        netstackid_t if_ipstack;   /* netns pointer on Linux */
        uintptr_t if_addr;         /* pointer to raw struct net_device */
} ifinfo_t;
```

**Note:**

DTrace translates the members of `ifinfo_t` from a `struct net_device *`.

### ipv4info\_t <a id="dt_ref_ipipv4_prov">

The `ipv4info_t` structure is translated version of the IPv4 header
(with raw pointer).
These values are NULL if the packet is not IPv4.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/ip.d`.
The definition of `ipv4info_t` is as follows:

```nocopybutton
typedef struct ipv4info {
        uint8_t ipv4_ver;               /* IP version (4) */
        uint8_t ipv4_ihl;               /* header length, bytes */
        uint8_t ipv4_tos;               /* type of service field */
        uint16_t ipv4_length;           /* length (header + payload) */
        uint16_t ipv4_ident;            /* identification */
        uint8_t ipv4_flags;             /* IP flags */
        uint16_t ipv4_offset;           /* fragment offset */
        uint8_t ipv4_ttl;               /* time to live */
        uint8_t ipv4_protocol;          /* next level protocol */
        string ipv4_protostr;           /* next level protocol, as string */
        uint16_t ipv4_checksum;         /* header checksum */
        ipaddr_t ipv4_src;              /* source address */
        ipaddr_t ipv4_dst;              /* destination address */
        string ipv4_saddr;              /* source address, string */
        string ipv4_daddr;              /* destination address, string */
        struct iphdr *ipv4_hdr;         /* pointer to raw header */
} ipv4info_t;
```

**Note:**

DTrace translates the members of `ipv4info_t` from a `struct iphdr *`.

### ipv6info\_t <a id="dt_ref_ipipv6_prov">

The `ipv6info_t` structure is translated version of the IPv6 header
(with raw pointer).
These values are NULL if the packet is not IPv6.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/ip.d`.
The definition of `ipv6info_t` is as follows:

```nocopybutton
typedef struct ipv6info {
        uint8_t ipv6_ver;               /* IP version (6) */
        uint8_t ipv6_tclass;            /* traffic class */
        uint32_t ipv6_flow;             /* flow label */
        uint16_t ipv6_plen;             /* payload length */
        uint8_t ipv6_nexthdr;           /* next header protocol */
        string ipv6_nextstr;            /* next header protocol, as string */
        uint8_t ipv6_hlim;              /* hop limit */
        in6_addr_t *ipv6_src;           /* source address */
        in6_addr_t *ipv6_dst;           /* destination address */
        string ipv6_saddr;              /* source address, string */
        string ipv6_daddr;              /* destination address, string */
        struct ipv6hdr *ipv6_hdr;       /* pointer to raw header */
} ipv6info_t;
```

**Note:**

DTrace translates the members of `ipv6info_t` from a `struct ipv6hdr *`.

## ip Stability <a id="dt_ref_ipstab_prov">

The `ip` provider uses DTrace's stability mechanism to describe its stabilities. These stability values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | ISA              |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Evolving       | Evolving       | ISA              |
| Arguments | Evolving       | Evolving       | ISA              |
