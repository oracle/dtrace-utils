# TCP Provider

The `tcp` provider makes available probes that mark different
phases of tcp processing.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## tcp Probes <a id="dt_ref_tcpprobes_prov">

`tcp` provides a probe for each of TCP accept (established or refused), connect
(request, established, refused), send, receive, and state-change:

- `accept-established`
- `accept-refused`
- `connect-request`
- `connect-established`
- `connect-refused`
- `send`
- `receive`
- `state-change`

The module name is always `vmlinux` and the function name is empty.

## tcp Probe Arguments <a id="dt_ref_tcpargs_prov">

The following table lists the argument types for `tcp` probes.

| probe                 | `args[0]`     | `args[1]`    | `args[2]`    | `args[3]`      | `args[4]`     | `args[5]`       |
| :---                  | :---          | :---         | :---         | :---           | :---          | :---            |
| `accept-established`  | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `tcpsinfo_t *` | `tcpinfo_t *` | `void`          |
| `accept-refused`      | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `tcpsinfo_t *` | `tcpinfo_t *` | `void`          |
| `connect-request`     | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `tcpsinfo_t *` | `tcpinfo_t *` | `void`          |
| `connect-established` | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `tcpsinfo_t *` | `tcpinfo_t *` | `void`          |
| `connect-refused`     | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `tcpsinfo_t *` | `tcpinfo_t *` | `void`          |
| `send`                | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `tcpsinfo_t *` | `tcpinfo_t *` | `void`          |
| `receive`             | `pktinfo_t *` | `csinfo_t *` | `ipinfo_t *` | `tcpsinfo_t *` | `tcpinfo_t *` | `void`          |
| `state-change`        | `void`        | `csinfo_t *` | `void`       | `tcpsinfo_t *` | `void`        | `tcplsinfo_t *` |

The `pktinfo_t`, `csinfo_t`, and `ipinfo_t` structures are described in [IP Provider](dtrace_providers_ip.md).

### tcpinfo\_t <a id="dt_ref_tcptcp_prov">

The `tcpinfo_t` structure contains TCP header fields.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/tcp.d`.
The definition of `tcpinfo_t` is as follows:

```nocopybutton
typedef struct tcpinfo {
        uint16_t tcp_sport;      /* source port */
        uint16_t tcp_dport;      /* destination port */
        uint32_t tcp_seq;        /* sequence number */
        uint32_t tcp_ack;        /* acknowledgment number */
        uint8_t tcp_offset;      /* data offset, in bytes */
        uint8_t tcp_flags;       /* flags */
        uint16_t tcp_window;     /* window size */
        uint16_t tcp_checksum;   /* checksum */
        uint16_t tcp_urgent;     /* urgent data pointer */
        uintptr_t tcp_hdr;       /* raw TCP header */
} tcpinfo_t;
```

**Note:**

DTrace translates the members of `tcpinfo_t` from a `struct tcphdr *`.

### tcpsinfo\_t <a id="dt_ref_tcptcps_prov">

The `tcpsinfo_t` structure contains stable TCP details from tcp_t.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/tcp.d`.
The definition of `tcpsinfo_t` is as follows:

```nocopybutton
typedef struct tcpsinfo {
        uintptr_t tcps_addr;
        int tcps_local;              /* is delivered locally, boolean */
        uint16_t tcps_lport;         /* local port */
        uint16_t tcps_rport;         /* remote port */
        string tcps_laddr;           /* local address, as a string */
        string tcps_raddr;           /* remote address, as a string */
        int tcps_state;              /* TCP state */
        uint32_t tcps_iss;           /* Initial sequence # sent */
        uint32_t tcps_suna;          /* sequence # sent but unacked */
        uint32_t tcps_snxt;          /* next sequence # to send */
        uint32_t tcps_rnxt;          /* next sequence # expected */
        uint32_t tcps_swnd;          /* send window size */
        int32_t tcps_snd_ws;         /* send window scaling */
        uint32_t tcps_rwnd;          /* receive window size */
        int32_t tcps_rcv_ws;         /* receive window scaling */
        uint32_t tcps_cwnd;          /* congestion window */
        uint32_t tcps_cwnd_ssthresh; /* threshold for congestion avoidance */
        uint32_t tcps_sack_snxt;     /* next SACK seq # for retransmission */
        uint32_t tcps_rto;           /* round-trip timeout, msec */
        uint32_t tcps_mss;           /* max segment size */
        int tcps_retransmit;         /* retransmit send event, boolean */
        uint32_t tcps_rtt;           /* smoothed avg round-trip time, msec */
        uint32_t tcps_rtt_sd;        /* standard deviation of RTT */
        uint32_t tcps_irs;           /* Initial recv sequence # */
} tcpsinfo_t;
```

**Note:**

DTrace translates the members of `tcpsinfo_t` from a `struct tcp_sock *`.

### tcplsinfo\_t <a id="dt_ref_tcptcpls_prov">

The `tcplsinfo_t` structure has the old tcp state for state changes.
Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/tcp.d`.
The definition of `tcplsinfo_t` is as follows:

```nocopybutton
typedef struct tcplsinfo {
        int tcps_state;        /* previous TCP state */
} tcplsinfo_t;
```

## tcp Stability <a id="dt_ref_tcpstab_prov">

The `tcp` provider uses DTrace's stability mechanism to describe its stabilities. These stability values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | ISA              |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Evolving       | Evolving       | ISA              |
| Arguments | Evolving       | Evolving       | ISA              |
