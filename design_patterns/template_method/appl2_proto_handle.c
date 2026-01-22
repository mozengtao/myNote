// TCP/IP协议栈处理框架
typedef struct {
    int (*process_packet)(Packet* pkt);
    
    // 模板步骤
    int (*verify_checksum)(Packet* pkt);
    int (*lookup_route)(Packet* pkt);
    int (*apply_policy)(Packet* pkt);
    int (*forward_packet)(Packet* pkt);
} ProtocolHandler;

// 不同协议（TCP、UDP、ICMP）实现不同的步骤