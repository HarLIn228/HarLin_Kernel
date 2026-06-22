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
int dns_resolve(const char* domain, unsigned char* out_ip);

int dhcp_request(void);

int tcp_connect_remote(unsigned char* ip, unsigned short port);
int tcp_send(int conn_id, unsigned char* data, int data_len);
int tcp_recv(int conn_id, unsigned char* buf, int max_len, int timeout);
void tcp_close_conn(int conn_id);

#endif
