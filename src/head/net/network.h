#ifndef NETWORK_H
#define NETWORK_H

#include "harlin_API.h"

#define MAX_TCP_CONN     8
#define MAX_TCP_SEND_SIZE 1500

#define TCP_STATE_CLOSED    0
#define TCP_STATE_SYN_SENT  1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_FIN_SENT  3

struct tcp_conn {
    int state;
    unsigned short sport;
    unsigned short dport;
    unsigned int seq_local;
    unsigned int seq_remote;
    unsigned char remote_ip[4];
    unsigned char remote_mac[6];
    int mac_resolved;
    unsigned char unacked_data[MAX_TCP_SEND_SIZE];
    int unacked_len;
    int unacked_in_flight;
    unsigned int unacked_seq;
};

int network_init(void);
int network_http_get(const char* host, const char* path);
int network_https_get(const char* host, const char* path);
int dns_resolve(const char* domain, unsigned char* out_ip);

int dhcp_request(void);

int tcp_connect_remote(unsigned char* ip, unsigned short port);
int tcp_send(int conn_id, unsigned char* data, int data_len);
int tcp_recv(int conn_id, unsigned char* buf, int max_len, int timeout);
void tcp_close_conn(int conn_id);

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0
#define ICMP_HEADER_SIZE  8

struct icmp_ping_result {
    int       received;
    int       rtt_ms;
    u16       reply_id;
    u16       reply_seq;
    u8        reply_ttl;
    u8        reply_ip[4];
};

int icmp_ping(unsigned char* dest_ip, u16 ident, u16 seq, int timeout, int payload_size,
              struct icmp_ping_result* out);

#define Harlin_NetInit                network_init
#define Harlin_HttpGetRaw             network_http_get
#define Harlin_HttpsGetRaw            network_https_get
#define Harlin_DnsResolve             dns_resolve
#define Harlin_DhcpRequest            dhcp_request
#define Harlin_TcpConnectRemote       tcp_connect_remote
#define Harlin_TcpSend                tcp_send
#define Harlin_TcpRecv                tcp_recv
#define Harlin_TcpCloseConn           tcp_close_conn
#define Harlin_IcmpPing               icmp_ping

#endif
