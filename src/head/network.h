#ifndef NETWORK_H
#define NETWORK_H

int network_init(void);
int network_http_get(const char* host, const char* path);
int dns_resolve(const char* domain, unsigned char* out_ip);

#endif
