[Layer Two Tunneling Protocol - Version 3 (L2TPv3)](https://datatracker.ietf.org/doc/html/rfc3931)  

- l2tp 通过单独的或者中间3层网络(如Internet)建立二层隧道。
- l2tp术语
	- L2TP
		- Layer 2 Tunneling Protocol
	- LAC
		- L2TP Access Concentrator
	- LNS
		- L2TP Network Server
	- L2TP Tunnel
		- 隧道是一条在LAC和LNS，或者端到端之间的逻辑连接，用来承载 PPP session。LAC-LNS 隧道可以承载多条 session，peer-to-peer 隧道只能承载1条PPP session。隧道一条控制连接以及0或多条 session，每条 session 承载一条加密的 PPP 链接。
	- L2TP Session
		- 当 client 和 LNS 之间建立端到端的PPP连接时，会建立一条LAC和LNS之间的L2TP session，同理， 在端到端之间会建立一条 L2TP session 用来承载 PPP连接。L2TP session 和 L2TP call 是一一对应的，一条 tunnelled PPP session对应一条 L2TP call。
- l2tp 消息类型
	- 控制消息
		- 控制消息用来建立和维护 tunnel，以及 session 管理
			- 控制消息用来建立LAC和LNS之间的通信，控制消息和数据消息使用同样的传输通道
			- AVP(Attribute Value Pair)用来构建用于建立，维护和拆除L2TP 隧道的控制消息
			- 部分控制消息不包含 AVP，被称为 ZLB(Zero Length Body)消息，这类控制消息只包含 L2TP  header，主要用来对某些控制消息就行ACK确认。
		- 控制L2TP tunnel的消息
			- Control Connection Management
				- 1  (SCCRQ)    Start-Control-Connection-Request
				- 2  (SCCRP)    Start-Control-Connection-Reply
				- 3  (SCCCN)    Start-Control-Connection-Connected
				- 4  (StopCCN)  Stop-Control-Connection-Notification
				- 6  (HELLO)    Hello
				- 20  (ACK)      Explicit Acknowledgement
		- 控制L2TP session(call)的消息
			- Call Management
				- 7  (OCRQ)     Outgoing-Call-Request
				- 8  (OCRP)     Outgoing-Call-Reply
				- 9  (OCCN)     Outgoing-Call-Connected
				- 10  (ICRQ)     Incoming-Call-Request
				- 11  (ICRP)     Incoming-Call-Reply
				- 12  (ICCN)     Incoming-Call-Connected
				- 14  (CDN)      Call-Disconnect-Notify
	- 数据消息
		- 数据消息用来封装 PPP 帧
- 两种类型的tunnel
	- LAC-LNS 隧道
		- ![image.png](../assets/image_1670483897484_0.png)
		- ![image.png](../assets/image_1670487539447_0.png){:height 333, :width 716}
		- LAC-LNS连接建立过程
			- ![image.png](../assets/image_1670487767498_0.png)
				- 建立tunnel
					- LAC发送SCCRQ消息
					- LNS发送SCCRP消息用来响应LAC发送的SCCRQ消息
						- 如果LAC或者LNS的AVP设置有误，会发送StopCCN拆除tunnel
					- LAC发送SCCCN消息
					- LNS发送ZLB ACK消息来响应LAC发送的SCCCN消息
				- 基于tunnel建立session
					- LAC发送ICRQ消息
					- LNS发送ICRP消息来响应LAC发送的ICRQ消息
						- 如果LAC或者LNS的设置有误，会发送CDN拆除tunnel
					- LAC发送ICCN消息
					- LNS发送ZLB ACK来响应LAC发送的ICCN消息，ZLB ACK可能位于另一个消息当中。此时l2tp session建立完成。
				- PPP协商在l2tp tunnel和l2tp session建立完成之后开始
					- 通过tunnel建立PPP连接的过程
						- ![image.png](../assets/image_1670491545134_0.png)
				- tunnel建立之后，LAC会周期性的发送keepalive消息给LNS，LNS会做出回应，如果LAC在一定时间内没有收到LNS的回应，它会认为tunnel断连并尝试重新建立tunnel连接。
	- peer-to-peer 隧道
		- 通过中间的IP网络建立单独的3层端到端VPN，用来建立端到端的PPP链路

[L2TP VPN基本原理](https://cshihong.github.io/2019/08/21/L2TP-VPN%E5%9F%BA%E6%9C%AC%E5%8E%9F%E7%90%86/)  
[L2TPv2 Feature Overview](https://www.alliedtelesis.com/sites/default/files/documents/configuration-guides/l2tp-tunnel_feature_config_guide_rev_c.pdf)  
[L2TPv2 RFC](https://www.rfc-editor.org/rfc/rfc2661.html)  
[L2TPv3 RFC](https://www.rfc-editor.org/rfc/rfc3931)  
[Cisco ISG Design and Deployment Guide: ATM to ISG LNS Aggregation](https://www.cisco.com/c/en/us/td/docs/ios/solutions_docs/edge_ios/dd_3_6.html)  
[Layer Two Tunneling Protocol - Version 3 (L2TPv3)](https://www.tech-invite.com/y35/tinv-ietf-rfc-3931.html)  
[L2TPv2 Feature Overview and Configuration Guide](https://www.alliedtelesis.com/sites/default/files/documents/configuration-guides/l2tp-tunnel_feature_config_guide_rev_c.pdf)  


```
The Layer Two Tunneling Protocol (L2TP) provides a dynamic mechanismfor tunneling Layer 2 (L2) "circuits" across a packet-oriented datanetwork (e.g., over IP).
L2TP has since been adopted for tunneling anumber of other L2 protocols.


Terminology:
Attribute Value Pair (AVP)
Call (Circuit Up)
    The action of transitioning a circuit on an L2TP Access Concentrator (LAC) to an "up" or "active" state.
    A call may be dynamically established through signaling properties or statically configured, A call is defined by its properties (e.g., type of call, called number, etc.) and its data traffic
Circuit
    A general term identifying any one of a wide range of L2 connections.
Client
Control Connection
    An L2TP control connection is a reliable control channel that is used to establish, maintain, and release individual L2TP sessions as well as the control connection itself.
Control Message
    An L2TP message used by the control connection
Data Message
    Message used by the data channel
Data Channel
    The channel for L2TP-encapsulated data traffic that passes between two LCCEs over a Packet-Switched Network
Incoming Call
    The action of receiving a call (circuit up event) on an LAC.
L2TP Access Concentrator (LAC)
    If an L2TP Control Connection Endpoint (LCCE) is being used to cross-connect an L2TP session directly to a data link, we refer to it as an L2TP Access Concentrator (LAC). An LCCE may act as both an L2TP Network Server (LNS) for some sessions and an LAC for others
L2TP Control Connection Endpoint (LCCE)
    An L2TP node that exists at either end of an L2TP control connection
L2TP Network Server (LNS)
    If a given L2TP session is terminated at the L2TP node and the encapsulated network layer (L3) packet processed on a virtual interface, we refer to this L2TP node as an L2TP Network Server (LNS).  A given LCCE may act as both an LNS for some sessions and an LAC for others
Outgoing Call
    The action of placing a call by an LAC, typically in response to policy directed by the peer in an Outgoing Call Request
Outgoing Call Request
    A request sent to an LAC to place an outgoing call. 
Packet-Switched Network (PSN)
    A network that uses packet switching technology for data delivery.
Peer
    When used in context with L2TP, Peer refers to the far end of an L2TP control connection (i.e., the remote LCCE).
Pseudowire (PW)
    An emulated circuit as it traverses a PSN. There is one Pseudowire per L2TP Session
Pseudowire Type
    The payload type being carried within an L2TP session.  Examples include PPP, Ethernet, and Frame Relay.
Remote System
    An end system or router connected by a circuit to an LAC
Session
    An L2TP session is the entity that is created between two LCCEs in order to exchange parameters for and maintain an emulated L2 connection.  Multiple sessions may be associated with a single Control Connection.
Zero-Length Body (ZLB) Message
    A control message with only an L2TP header. 


L2TP 2 types of messages
1.control messages (control packets)
    Control messages are used in the establishment, maintenance, and clearing of control connections and sessions. These messages utilize a reliable control channel within L2TP to guarantee delivery
    These control messages are used in conjunction with the associated protocol state machines that govern the dynamic setup, maintenance, and teardown for L2TP sessions.
2.data messages (data packets)
    Data messages are used to encapsulate the L2 traffic being carried over the L2TP session.  Unlike control messages, data messages are not retransmitted when packet loss occurs.

L2TPv3 Structure
+-------------------+    +-----------------------+
| Tunneled Frame    |    | L2TP Control Message  |
+-------------------+    +-----------------------+
| L2TP Data Header  |    | L2TP Control Header   |
+-------------------+    +-----------------------+
| L2TP Data Channel |    | L2TP Control Channel  |
| (unreliable)      |    | (reliable)            |
+-------------------+----+-----------------------+
| Packet-Switched Network (IP, FR, MPLS, etc.)   |
+------------------------------------------------+


The necessary setup for tunneling a session with L2TP:
(1) Establishing the control connection
(2) Establishing a session as triggered by an incoming call or outgoing call.
An L2TP session MUST be established before L2TP can begin to forward session frames.  Multiple sessions may be bound to a single control connection, and multiple control connections may exist between the same two LCCEs.

Control Message Types:
   Control Connection Management
       0  (reserved)
       1  (SCCRQ)    Start-Control-Connection-Request
       2  (SCCRP)    Start-Control-Connection-Reply
       3  (SCCCN)    Start-Control-Connection-Connected
       4  (StopCCN)  Stop-Control-Connection-Notification
       5  (reserved)
       6  (HELLO)    Hello
      20  (ACK)      Explicit Acknowledgement
   Call Management
       7  (OCRQ)     Outgoing-Call-Request
       8  (OCRP)     Outgoing-Call-Reply
       9  (OCCN)     Outgoing-Call-Connected
      10  (ICRQ)     Incoming-Call-Request
      11  (ICRP)     Incoming-Call-Reply
      12  (ICCN)     Incoming-Call-Connected
      13  (reserved)
      14  (CDN)      Call-Disconnect-Notify

   Error Reporting
      15  (WEN)      WAN-Error-Notify

   Link Status Change Reporting
      16  (SLI)      Set-Link-Info

The Message Type AVP (see Section 5.4.1) defines the specific type of control message being sent


The L2TP control message header provides information for the reliable transport of messages that govern the establishment, maintenance, and teardown of L2TP sessions.  By default, control messages are sent over the underlying media in-band with L2TP data messages.


L2TP Control Message Header:
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |T|L|x|x|S|x|x|x|x|x|x|x|  Ver  |             Length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                     Control Connection ID                     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               Ns              |               Nr              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
The T bit MUST be set to 1, indicating that this is a control message.
The L and S bits MUST be set to 1, indicating that the Length field and sequence numbers are present.
The x bits are reserved for future extensions.  All reserved bits MUST be set to 0 on outgoing messages and ignored on incoming messages.
The Ver field indicates the version of the L2TP control message header
The Length field indicates the total length of the message in octets, always calculated from the start of the control message header itself (beginning with the T bit).
The Control Connection ID field contains the identifier for the control connection.  L2TP control connections are named by identifiers that have local significance only. 
Ns indicates the sequence number for this control message
Nr indicates the sequence number expected in the next control messageto be received. 


Control Connection Management:
The L2TP control connection handles dynamic establishment, teardown, and maintenance of the L2TP sessions and of the control connection itself.

LCCEs are identified during control connection establishment either by the Host Name AVP, the Router ID AVP, or a combination of the two (see Section 5.4.3). The identity of a peer LCCE is central to selecting proper configuration parameters (i.e., Hello interval, window size, etc.) for a control connection, as well as for determining how to set up associated sessions within the control connection, password lookup for control connection authentication, control connection level tie breaking, etc.

Control Connection Establishment
Establishment of the control connection involves an exchange of AVPs that identifies the peer and its capabilities.
      LCCE A      LCCE B
      ------      ------
      SCCRQ ->
                  <- SCCRP
      SCCCN ->

Control Connection Teardown
Control connection teardown may be initiated by either LCCE and is accomplished by sending a single StopCCN control message. 
      LCCE A      LCCE B
      ------      ------
      StopCCN ->
      (Clean up)

                  (Wait)
                  (Clean up)
 An implementation may shut down an entire control connection and all sessions associated with the control connection by sending the StopCCN.


Session Management
After successful control connection establishment, individual sessions may be created.  Each session corresponds to a single data stream between the two LCCEs. 

Session Establishment for an Incoming Call
      LCCE A      LCCE B
      ------      ------
      (Call
       Detected)

      ICRQ ->
                 <- ICRP
      (Call
       Accepted)

      ICCN ->

Session Teardown
Session teardown may be initiated by either the LAC or LNS and is accomplished by sending a CDN control message. After the last session is cleared, the control connection MAY be torn down as well (and typically is)
      LCCE A      LCCE B
      ------      ------
      CDN ->
      (Clean up)

                  (Clean up)


field definitions defined for all L2TP Session Header encapsulations
Session ID
    A 32-bit field containing a non-zero identifier for a session.
    L2TP sessions are named by identifiers that have local significance only. The Session ID alone provides the necessary context for all further packet processing
Cookie
    The optional Cookie field contains a variable-length value (maximum 64 bits) used to check the association of a received data message with the session identified by the Session ID.
    The Cookie provides an additional level of guarantee that a data message has been directed to the proper session by the Session ID.
    When the L2TP control connection is used for session establishment, random Cookie values are selected and exchanged as Assigned Cookie AVPs during session creation.


Control Message Attribute Value Pairs：
Each AVP is encoded as follows:
                          Figure 5.1: AVP Format

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |M|H| rsvd  |      Length       |           Vendor ID           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |         Attribute Type        |        Attribute Value ...
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                       (until Length is reached)                   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

The first six bits comprise a bit mask that describes the general attributes of the AVP.

Mandatory (M) bit: Controls the behavior required of an implementation that receives an unrecognized AVP.
Hidden (H) bit: Identifies the hiding of data in the Attribute Value field of an AVP.
Length: Contains the number of octets (including the Overall Length and bit mask fields) contained in this AVP. The Length may be calculated as 6 + the length of the Attribute Value field in octets. The field itself is 10 bits, permitting a maximum of 1023 octets of data in a single AVP.  The minimum Length of an AVP is 6.  If the Length is 6, then the Attribute Value field is absent.
Vendor ID:
Attribute Type: A 2-octet value with a unique interpretation across all AVPs defined under a given Vendor ID.
Attribute Value: This is the actual value as indicated by the Vendor ID and Attribute Type. 


L2TP Data Message:
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                      L2TP Session Header                      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                      L2-Specific Sublayer                     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Tunnel Payload                      ...
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
The L2TP Session Header is specific to the encapsulating PSN over which the L2TP traffic is delivered. The Session Header MUST provide (1) a method of distinguishing traffic among multiple L2TP data sessions and (2) a method of distinguishing data messages from control messages. Each type of encapsulating PSN MUST define its own session header, clearly identifying the format of the header and parameters necessary to setup the session.
The L2-Specific Sublayer is an intermediary layer between the L2TP session header and the start of the tunneled frame.
The Data Message Header is followed by the Tunnel Payload, including any necessary L2 framing as defined in the payload-specific companion documents.
The Data Message Header is followed by the Tunnel Payload, including any necessary L2 framing as defined in the payload-specific companion documents.


              Figure 4.6: Default L2-Specific Sublayer Format

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |x|S|x|x|x|x|x|x|              Sequence Number                  |

The Sequence Number field may be used to detect lost, duplicate, or out-of-order packets within a given session.



L2TPv3 over IP:
L2TPv3 over IP (both versions) utilizes the IANA-assigned IP protocol ID 115.
1. without UDP
               Figure 4.1.1.1: L2TPv3 Session Header Over IP

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           Session ID                          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               Cookie (optional, maximum 64 bits)...
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                                   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


           Figure 4.1.1.2: L2TPv3 Control Message Header Over IP

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                      (32 bits of zeros)                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |T|L|x|x|S|x|x|x|x|x|x|x|  Ver  |             Length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                     Control Connection ID                     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               Ns              |               Nr              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 When operating directly over IP, L2TP packets lose the ability to take advantage of the UDP checksum as a simple packet integrity check, which is of particular concern for L2TP control messages.
 L2TP over IP is not as NAT-friendly as L2TP over UDP

2.L2TP over UDP
              Figure 4.1.2.1: L2TPv3 Session Header over UDP

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |T|x|x|x|x|x|x|x|x|x|x|x|  Ver  |          Reserved             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           Session ID                          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               Cookie (optional, maximum 64 bits)...
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                                   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
The T bit MUST be set to 0, indicating that this is a data message.







General Control Message AVPs:

Message Type (All Messages)
The Message Type AVP, Attribute Type 0, identifies the control message herein
Attribute Value field format:
       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |         Message Type          |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
The Message Type AVP MUST be the first AVP in a message, immediately following the control message header

Message Digest (All Messages)
The Message Digest AVP, Attribute Type 59 is used as an integrity and authentication check of the L2TP Control Message header and body
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  Digest Type  | Message Digest ...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                        ... (16 or 20 octets)         |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


Control Message Authentication Nonce (SCCRQ, SCCRP)
Random Vector (All Messages)


Result and Error Codes：
Result Code (StopCCN, CDN)
The Result Code AVP, Attribute Type 1, indicates the reason for terminating the control connection or session.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |          Result Code          |     Error Code (optional)     |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | Error Message ... (optional, arbitrary number of octets)      |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


Control Connection Management AVPs:

Control Connection Tie Breaker (SCCRQ)
The Control Connection Tie Breaker AVP, Attribute Type 5, indicates that the sender desires a single control connection to exist between a given pair of LCCEs.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | Control Connection Tie Breaker Value ...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                 ... (64 bits)        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


Host Name (SCCRQ, SCCRP)
The Host Name AVP, Attribute Type 7, indicates the name of the issuing LAC or LNS

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | Host Name ... (arbitrary number of octets)
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Router ID (SCCRQ, SCCRP)
The Router ID AVP, Attribute Type 60, is an identifier used to identify an LCCE for control connection setup, tie breaking, and/or tunnel authentication.
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                      Router Identifier                        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Vendor Name (SCCRQ, SCCRP)
The Vendor Name AVP, Attribute Type 8, contains a vendor-specific (possibly human-readable) string describing the type of LAC or LNS being used.
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  Vendor Name ... (arbitrary number of octets)
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Receive Window Size (SCCRQ, SCCRP)
The Receive Window Size AVP, Attribute Type 10, specifies the receive window size being offered to the remote peer.

       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |         Window Size           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Pseudowire Capabilities List (SCCRQ, SCCRP)
The Pseudowire Capabilities List (PW Capabilities List) AVP, Attribute Type 62, indicates the L2 payload types the sender can support.  The specific payload type of a given session is identified by the Pseudowire Type AVP.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |           PW Type 0           |             ...               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |              ...              |          PW Type N            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


 Preferred Language (SCCRQ, SCCRP)


 Session Management AVPs:

Local Session ID (ICRQ, ICRP, ICCN, OCRQ, OCRP, OCCN, CDN, WEN, SLI)
The Local Session ID AVP , Attribute Type 63, contains the identifier being assigned to this session by the sender.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Local Session ID                        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Remote Session ID (ICRQ, ICRP, ICCN, OCRQ, OCRP, OCCN, CDN, WEN, SLI)
The Remote Session ID AVP, Attribute Type 64, contains the identifier that was assigned to this session by the peer.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                      Remote Session ID                        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
The Remote Session ID AVP MUST be present in all session-level control messages. 
In most cases, this identifier is sufficient for the peer to look up session-level context for this control message.

Assigned Cookie (ICRQ, ICRP, OCRQ, OCRP)
The Assigned Cookie AVP, Attribute Type 65, contains the Cookie value being assigned to this session by the sender.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |               Assigned Cookie (32 or 64 bits) ...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Serial Number (ICRQ, OCRQ)
The Serial Number AVP, Attribute Type 15, contains an identifier assigned by the LAC or LNS to this session.

       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                        Serial Number                          |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 Remote End ID (ICRQ, OCRQ)
 The Remote End ID AVP, Attribute Type 66, contains an identifier used to bind L2TP sessions to a given circuit, interface, or bridging instance.  It also may be used to detect session-level ties.
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | Remote End Identifier ... (arbitrary number of octets)
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Session Tie Breaker (ICRQ, OCRQ)
The Session Tie Breaker AVP, Attribute Type 5, is used to break ties when two peers concurrently attempt to establish a session for the same circuit.

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | Session Tie Breaker Value ...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                 ... (64 bits)        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Pseudowire Type (ICRQ, OCRQ)
The Pseudowire Type (PW Type) AVP, Attribute Type 68, indicates the L2 payload type of the packets that will be tunneled using this L2TP session.
       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |           PW Type             |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

L2-Specific Sublayer (ICRQ, ICRP, ICCN, OCRQ, OCRP, OCCN)
The L2-Specific Sublayer AVP, Attribute Type 69, indicates the presence and format of the L2-Specific Sublayer the sender of this AVP requires on all incoming data packets for this L2TP session.

       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |   L2-Specific Sublayer Type   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Data Sequencing (ICRQ, ICRP, ICCN, OCRQ, OCRP, OCCN)
The Data Sequencing AVP, Attribute Type 70, indicates that the sender requires some or all of the data packets that it receives to be sequenced.

       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |     Data Sequencing Level     |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Physical Channel ID (ICRQ, ICRP, OCRP)
The Physical Channel ID AVP, Attribute Type 25, contains the vendor-specific physical channel number used for a call.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                      Physical Channel ID                      |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


Circuit Status AVPs

Circuit Status (ICRQ, ICRP, ICCN, OCRQ, OCRP, OCCN, SLI)
The Circuit Status AVP, Attribute Type 71, indicates the initial status of or a status change in the circuit to which the session is bound.

       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |         Reserved          |N|A|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Circuit Errors (WEN)
The Circuit Errors AVP, Attribute Type 34, conveys circuit error information to the peer.



Control Connection Protocol Specification:

Start-Control-Connection-Request (SCCRQ)
Start-Control-Connection-Request (SCCRQ) is a control message used to initiate a control connection between two LCCEs.

AVPs MUST be present in the SCCRQ:
      Message Type
      Host Name
      Router ID
      Assigned Control Connection ID
      Pseudowire Capabilities List
AVPs MAY be present in the SCCRQ:
      Random Vector
      Control Message Authentication Nonce
      Message Digest
      Control Connection Tie Breaker
      Vendor Name
      Receive Window Size
      Preferred Language


Start-Control-Connection-Reply (SCCRP)
Start-Control-Connection-Reply (SCCRP) is the control message sent in reply to a received SCCRQ message.  The SCCRP is used to indicate that the SCCRQ was accepted and that establishment of the control connection should continue.

AVPs MUST be present in the SCCRP:
      Message Type
      Host Name
      Router ID
      Assigned Control Connection ID
      Pseudowire Capabilities List
AVPs MAY be present in the SCCRP:
      Random Vector
      Control Message Authentication Nonce
      Message Digest
      Vendor Name
      Receive Window Size
      Preferred Language


Start-Control-Connection-Connected (SCCCN)
Start-Control-Connection-Connected (SCCCN) is the control message sent in reply to an SCCRP.  The SCCCN completes the control connection establishment process.

AVP MUST be present in the SCCCN:
      Message Type

AVP MAY be present in the SCCCN:
      Random Vector
      Message Digest


Stop-Control-Connection-Notification (StopCCN)
Stop-Control-Connection-Notification (StopCCN) is the control message sent by either LCCE to inform its peer that the control connection is being shut down and that the control connection should be closed. In addition, all active sessions are implicitly cleared (without sending any explicit session control messages).

AVPs MUST be present in the StopCCN:
      Message Type
      Result Code
AVPs MAY be present in the StopCCN:
      Random Vector
      Message Digest
      Assigned Control Connection ID


Hello (HELLO)
The Hello (HELLO) message is an L2TP control message sent by either peer of a control connection.  This control message is used as a "keepalive" for the control connection.  See Section 4.2 for a description of the keepalive mechanism.

HELLO messages are global to the control connection.  The Session ID in a HELLO message MUST be 0.
AVP MUST be present in the HELLO:
      Message Type

AVP MAY be present in the HELLO:
      Random Vector
      Message Digest


Incoming-Call-Request (ICRQ)
Incoming-Call-Request (ICRQ) is the control message sent by an LCCE to a peer when an incoming call is detected，It is the first in a three-message exchange used for establishing a session via an L2TP control connection.
AVPs MUST be present in the ICRQ:

      Message Type
      Local Session ID
      Remote Session ID
      Serial Number
      Pseudowire Type
      Remote End ID
      Circuit Status

AVPs MAY be present in the ICRQ:

      Random Vector
      Message Digest
      Assigned Cookie
      Session Tie Breaker
      L2-Specific Sublayer
      Data Sequencing
      Tx Connect Speed
      Rx Connect Speed
      Physical Channel ID

Incoming-Call-Reply (ICRP)
Incoming-Call-Reply (ICRP) is the control message sent by an LCCE in response to a received ICRQ. 
AVPs MUST be present in the ICRP:

      Message Type
      Local Session ID
      Remote Session ID
      Circuit Status

AVPs MAY be present in the ICRP:

      Random Vector
      Message Digest
      Assigned Cookie
      L2-Specific Sublayer
      Data Sequencing
      Tx Connect Speed
      Rx Connect Speed
      Physical Channel ID

Incoming-Call-Connected (ICCN)
Incoming-Call-Connected (ICCN) is the control message sent by the LCCE that originally sent an ICRQ upon receiving an ICRP from its peer.
The ICCN is used to indicate that the ICRP was accepted, that the call has been established, and that the L2TP session should move to the established state.
AVPs MUST be present in the ICCN:

      Message Type
      Local Session ID
      Remote Session ID

AVPs MAY be present in the ICCN:

      Random Vector
      Message Digest
      L2-Specific Sublayer
      Data Sequencing
      Tx Connect Speed
      Rx Connect Speed
      Circuit Status


Outgoing-Call-Request (OCRQ)
Outgoing-Call-Reply (OCRP)
Outgoing-Call-Connected (OCCN)

Call-Disconnect-Notify (CDN)
The Call-Disconnect-Notify (CDN) is a control message sent by an LCCE to request disconnection of a specific session.  Its purpose is to inform the peer of the disconnection and the reason for the disconnection.  The peer MUST clean up any resources, and does not send back any indication of success or failure for such cleanup.
AVPs MUST be present in the CDN:

      Message Type
      Result Code
      Local Session ID
      Remote Session ID

AVP MAY be present in the CDN:

      Random Vector
      Message Digest



WAN-Error-Notify (WEN)


Set-Link-Info (SLI)
The Set-Link-Info control message is sent by an LCCE to convey link or circuit status change information regarding the circuit associated with this L2TP session.

AVPs MUST be present in the SLI:

      Message Type
      Local Session ID
      Remote Session ID

AVPs MAY be present in the SLI:

      Random Vector
      Message Digest
      Circuit Status


Explicit-Acknowledgement (ACK)
The Explicit Acknowledgement (ACK) message is used only to acknowledge receipt of a message or messages on the control connection (e.g., for purposes of updating Ns and Nr values).
AVPs MAY be present in the ACK message:

      Message Type
      Message Digest


Control Connection States

   State           Event              Action              New State
   -----           -----              ------              ---------
   idle            Local open         Send SCCRQ          wait-ctl-reply
                   request

   idle            Receive SCCRQ,     Send SCCRP          wait-ctl-conn
                   acceptable

   idle            Receive SCCRQ,     Send StopCCN,       idle
                   not acceptable     clean up

   idle            Receive SCCRP      Send StopCCN,       idle
                                      clean up

   idle            Receive SCCCN      Send StopCCN,       idle
                                      clean up

   wait-ctl-reply  Receive SCCRP,     Send SCCCN,         established
                   acceptable         send control-conn
                                      open event to
                                      waiting sessions

   wait-ctl-reply  Receive SCCRP,     Send StopCCN,       idle
                   not acceptable     clean up

   wait-ctl-reply  Receive SCCRQ,     Send SCCRP,         wait-ctl-conn
                   lose tie breaker,  Clean up losing
                   SCCRQ acceptable   connection

   wait-ctl-reply  Receive SCCRQ,     Send StopCCN,       idle
                   lose tie breaker,  Clean up losing
                   SCCRQ unacceptable connection

   wait-ctl-reply  Receive SCCRQ,     Send StopCCN for    wait-ctl-reply
                   win tie breaker    losing connection

   wait-ctl-reply  Receive SCCCN      Send StopCCN,       idle
                                      clean up

   wait-ctl-conn   Receive SCCCN,     Send control-conn   established
                   acceptable         open event to
                                      waiting sessions

   wait-ctl-conn   Receive SCCCN,     Send StopCCN,       idle
                   not acceptable     clean up

   wait-ctl-conn   Receive SCCRQ,     Send StopCCN,       idle
                   SCCRP              clean up

   established     Local open         Send control-conn   established
                   request            open event to
                   (new call)         waiting sessions

   established     Administrative     Send StopCCN,       idle
                   control-conn       clean up
                   close event

   established     Receive SCCRQ,     Send StopCCN,       idle
                   SCCRP, SCCCN       clean up

   idle,           Receive StopCCN    Clean up            idle
   wait-ctl-reply,
   wait-ctl-conn,
   established


ICRQ Sender States

   State           Event              Action           New State
   -----           -----              ------           ---------

   idle            Call signal or     Initiate local   wait-control-conn
                   ready to receive   control-conn
                   incoming conn      open

   idle            Receive ICCN,      Clean up         idle
                   ICRP, CDN

   wait-control-   Bearer line drop   Clean up         idle
   conn            or local close
                   request

   wait-control-   control-conn-open  Send ICRQ        wait-reply
   conn

   wait-reply      Receive ICRP,      Send ICCN        established
                   acceptable

   wait-reply      Receive ICRP,      Send CDN,        idle
                   Not acceptable     clean up

   wait-reply      Receive ICRQ,      Process as       idle
                   lose tie breaker   ICRQ Recipient
                                      (Section 7.3.2)

   wait-reply      Receive ICRQ,      Send CDN         wait-reply
                   win tie breaker    for losing
                                      session

   wait-reply      Receive CDN,       Clean up         idle
                   ICCN

   wait-reply      Local close        Send CDN,        idle
                   request            clean up

   established     Receive CDN        Clean up         idle

   established     Receive ICRQ,      Send CDN,        idle
                   ICRP, ICCN         clean up

   established     Local close        Send CDN,        idle
                   request            clean up


   established
      Data is exchanged over the session.  The call may be cleared by
      any of the following:
         + An event on the connected interface: The LCCE sends a CDN.
         + Receipt of a CDN: The LCCE cleans up, disconnecting the call.
         + A local reason: The LCCE sends a CDN.

ICRQ Recipient States

   State           Event              Action            New State
   -----           -----              ------            ---------
   idle            Receive ICRQ,      Send ICRP         wait-connect
                   acceptable

   idle            Receive ICRQ,      Send CDN,         idle
                   not acceptable     clean up

   idle            Receive ICRP       Send CDN          idle
                                      clean up

   idle            Receive ICCN       Clean up          idle

   wait-connect    Receive ICCN,      Prepare for       established
                   acceptable         data

   wait-connect    Receive ICCN,      Send CDN,         idle
                   not acceptable     clean up

   wait-connect    Receive ICRQ,      Send CDN,         idle
                   ICRP               clean up

   idle,           Receive CDN        Clean up          idle
   wait-connect,
   established

   wait-connect    Local close        Send CDN,         idle
   established     request            clean up

   established     Receive ICRQ,      Send CDN,         idle
                   ICRP, ICCN         clean up


OCRQ Sender States
OCRQ Recipient (LAC) States


Termination of a Control Connection

The termination of a control connection consists of either peer issuing a StopCCN.  The sender of this message SHOULD wait a full control message retransmission cycle (e.g., 1 + 2 + 4 + 8 ... seconds) for the acknowledgment of this message before releasing the control information associated with the control connection.  The recipient of this message should send an acknowledgment of the message to the peer, then release the associated control information.


Control Message Attribute Value Pairs
      Attribute
      Type        Description
      ---------   ------------------

         58       Extended Vendor ID AVP
         59       Message Digest
         60       Router ID
         61       Assigned Control Connection ID
         62       Pseudowire Capabilities List
         63       Local Session ID
         64       Remote Session ID
         65       Assigned Cookie
         66       Remote End ID
         68       Pseudowire Type
         69       L2-Specific Sublayer
         70       Data Sequencing
         71       Circuit Status
         72       Preferred Language
         73       Control Message Authentication Nonce
         74       Tx Connect Speed
         75       Rx Connect Speed

Message Type AVP Values
   Message Type AVP (Attribute Type 0) Values
   ------------------------------------------

     Control Connection Management

         20 (ACK)  Explicit Acknowledgement


Result Code AVP Values
   Result Code AVP (Attribute Type 1) Values
   -----------------------------------------

      General Error Codes

         13 - Session not established due to losing
              tie breaker (L2TPv3).
         14 - Session not established due to unsupported
              PW type (L2TPv3).
         15 - Session not established, sequencing required
              without valid L2-Specific Sublayer (L2TPv3).
         16 - Finite state machine error or timeout.


 Forwarding Session Data Frames
 Once session establishment is complete, circuit frames are received at an LCCE, encapsulated in L2TP and forwarded over the appropriate session. 
 For every outgoing data message, the sender places the identifier specified in the Local Session ID AVP  (received from peer during session establishment) in the Session ID field of the L2TP data header.
 The peer LCCE receiving the L2TP data packet identifies the session with which the packet is associated by the Session ID in the data packet's header. 
```