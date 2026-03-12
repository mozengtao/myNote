[PacketLife captures](https://packetlife.net/captures/)  
[SampleCaptures](https://wiki.wireshark.org/SampleCaptures#sample-captures)  
[Tshark LUA SCRIPTS](https://tshark.dev/packetcraft/scripting/lua_scripts/)  
[Wireshark Developer’s Guide](https://www.wireshark.org/docs/wsdg_html_chunked/)  
[Functions For New Protocols And Dissectors](https://www.wireshark.org/docs/wsdg_html_chunked/lua_module_Proto.html)  
[Lua Support in Wireshark](https://www.wireshark.org/docs/wsdg_html_chunked/wsluarm.html)  
[Wireshark’s Lua API Reference Manual](https://www.wireshark.org/docs/wsdg_html_chunked/wsluarm_modules.html)  
[wireshark-filter](https://www.wireshark.org/docs/man-pages/wireshark-filter.html)  
[dumpcap(1)](https://www.wireshark.org/docs/man-pages/dumpcap.html)  
[tshark(1)](https://www.wireshark.org/docs/man-pages/tshark.html)  
[mergecap(1)](https://www.wireshark.org/docs/man-pages/mergecap.html)  
[Wireshark User’s Guide](https://www.wireshark.org/docs/wsug_html_chunked/index.html)  
[Lua/Examples](https://wiki.wireshark.org/Lua/Examples)  
[Wireshark User's Guide](https://cse.sc.edu/~pokeefe/tutorials/wireshark/index.html)  
[Create Wireshark Dissector in Lua](https://cq674350529.github.io/2020/09/03/Create-Wireshark-Dissector-in-Lua/)  
[Wireshark解析器Lua脚本](https://kiosk007.top/post/wireshark%E8%A7%A3%E6%9E%90%E5%99%A8lua%E8%84%9A%E6%9C%AC/)  

## Creating a Wireshark dissector in Lua
[part 1](https://mika-s.github.io/wireshark/lua/dissector/2017/11/04/creating-a-wireshark-dissector-in-lua-1.html)  
[part 2](https://mika-s.github.io/wireshark/lua/dissector/2017/11/06/creating-a-wireshark-dissector-in-lua-2.html)  
[part 3](https://mika-s.github.io/wireshark/lua/dissector/2017/11/08/creating-a-wireshark-dissector-in-lua-3.html)  
[part 4](https://mika-s.github.io/wireshark/lua/dissector/2018/12/16/creating-a-wireshark-dissector-in-lua-4.html)  
[part 5](https://mika-s.github.io/wireshark/lua/dissector/2018/12/18/creating-a-wireshark-dissector-in-lua-5.html)  

## tshark
[TShark: The Basics Room](https://dev.to/dhwanit25/tshark-the-basics-room-46il)  
[tshark tutorial and filter examples](https://hackertarget.com/tshark-tutorial-and-filter-examples/)  
[Beginners Guide to TShark (Part 1)](https://www.hackingarticles.in/beginners-guide-to-tshark-part-1/)  
[Beginners Guide to TShark (Part 2)](https://www.hackingarticles.in/beginners-guide-to-tshark-part-2/)  
[https://linuxsimba.github.io/tshark-examples](https://linuxsimba.github.io/tshark-examples)  
[A quick tutorial on using tshark](https://www.yenolam.com/writings/tshark.pdf)  
> work with packet captures on the command-line
[tshark.dev](https://tshark.dev/)  
```bash
sudo tshark -D

sudo tshark -c 500 -w mycaptures.pcap

sudo tshark -w /tmp/dhcp.pcap -f "port 67 or port 68" -i eth1 -P

tshark -r test.pcap 

tshark -r test.pcap bootp.option.dhcp == 1

tshark -r test.pcap -T json

capinfos test.pcap

morrism~> head -c 24 test.pcap | hexdump -C
00000000  d4 c3 b2 a1 02 00 04 00  00 00 00 00 00 00 00 00  |................|
00000010  00 00 04 00 01 00 00 00                           |........|
00000018
```

## Standard PCAP (legacy) File Structure
[PCAP Capture File Format](https://datatracker.ietf.org/doc/draft-ietf-opsawg-pcap/)  
[PCAP Capture File Format](https://ietf-opsawg-wg.github.io/draft-ietf-opsawg-pcap/draft-ietf-opsawg-pcap.html)  
[Libpcap File Format](https://wiki.wireshark.org/Development/LibpcapFileFormat)  
[The Ethertype Value Identifier](https://www.liveaction.com/glossary/ethertype-values/)  
[etypes.h File Reference](https://www.wireshark.org/docs/wsar_html/etypes_8h.html)  
[SampleCaptures](https://wiki.wireshark.org/SampleCaptures)  
[]()  
```
+---------------------------------------------------------------+
|                Global Header (Fixed: 24 Bytes)                |
+-----------------------+-----------------------+---------------+
|     Magic Number      |  Major Ver |  Minor Ver|   Timezone   |
|       (4 bytes)       |  (2 bytes) |  (2 bytes) |   (4 bytes) |
+-----------+-----------+-----------+-----------+---------------+
|  Sigfigs  |  Snaplen  |        Network / LinkType             |
| (4 bytes) | (4 bytes) |             (4 bytes)                 |
+-----------+-----------+---------------------------------------+
|                                                               |
|                Packet #1 Header (16 Bytes)                    |
+-----------------------+-----------------------+---------------+
|   Timestamp Seconds   | Timestamp Microseconds|  Incl. Length |
|       (4 bytes)       |       (4 bytes)       |   (4 bytes)   |
+-----------+-----------+-----------------------+---------------+
|  Orig. Len|                                                   |
| (4 bytes) |           Packet #1 Data (Raw Bytes)              |
+-----------+        (Length defined by Incl. Length)           |
|                                                               |
+---------------------------------------------------------------+
|                                                               |
|                Packet #2 Header (16 Bytes)                    |
+-----------------------+-----------------------+---------------+
|   Timestamp Seconds   | Timestamp Microseconds|  Incl. Length |
|       (4 bytes)       |       (4 bytes)       |   (4 bytes)   |
+-----------+-----------+-----------------------+---------------+
|  Orig. Len|                                                   |
| (4 bytes) |           Packet #2 Data (Raw Bytes)              |
+-----------+                                                   |
|           (Repeat for all packets in the file...)             |
+---------------------------------------------------------------+
```


```c
// GLOBAL HEADER
typedef struct pcap_hdr_s {
        guint32 magic_number;   /* magic number */
        guint16 version_major;  /* major version number */
        guint16 version_minor;  /* minor version number */
        gint32  thiszone;       /* GMT to local correction */
        guint32 sigfigs;        /* accuracy of timestamps */
        guint32 snaplen;        /* max length of captured packets, in octets */
        guint32 network;        /* data link type */
} pcap_hdr_t;

Header size = 24 bytes:

magic_number = 4 bytes (d4 c3 b2 a1)
version_major = 2 bytes (02 00)
version_minor = 2 bytes (04 00) // in our case 2.4. (little endian)
thiszone = 4 bytes (00 00 00 00) // usually set to 0
sigfigs = 4 bytes (00 00 00 00) // usually set to 0
snaplen = 4 bytes (FF FF 00 00) // maximum length of the captured packets (data#) in bytes, here its 65535 (0xffff) which is default value for tcpdump and wireshark)
network = 4 bytes (01 00 00 00) // 0x1 which indicates that the link-layer protocol is Ethernet. Full list: http://www.tcpdump.org/linktypes.html

-------------------------------------
// Packet header
typedef struct pcaprec_hdr_s {
        guint32 ts_sec;         /* timestamp seconds */
        guint32 ts_usec;        /* timestamp microseconds */
        guint32 incl_len;       /* number of octets of packet saved in file */
        guint32 orig_len;       /* actual length of packet */
} pcaprec_hdr_t;

Packet header size = 16 bytes

ts_sec = 4 bytes (85 AD C7 50) // This is the number of seconds since the start of 1970, also known as Unix Epoch
ts_usec = 4 bytes (AC 97 05 00) // microseconds part of the time at which the packet was captured
incl_len = 4 bytes (E0 04 00 00) = 1248 // contains the size of the saved packet data in our file in bytes (following the header)
orig_len = 4 bytes (E0 04 00 00) // Both fields' value is same here, but these may have different values in cases where we set the maximum packet length (whose value is 65535 in the global header of our file) to a smaller size.

----------------------------------------
// Packet data
// The actual packet data will immediately follow the packet header as a data blob of incl_len bytes without a specific byte alignment

// Ether header structure
struct ether_header {
        u_char ether_dhost[6]; // destination host
       u_char ether_shost[6]; // source host
       u_short ether_type; // 2 bytes, Protocol type, type of Packet: ARP, DOD(IPv4), IPv6,.. http://www.networksorcery.com/enp/protocol/802/ethertypes.htm
}

// UDP header
typedef struct udp_header{
    u_short sport;          // Source port
    u_short dport;          // Destination port
    u_short len;            // Datagram length
    u_short crc;            // Checksum
}udp_header;
```