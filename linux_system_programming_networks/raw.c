#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static uint16_t csum(const void *buf, size_t len) {
    uint32_t sum = 0; const uint16_t *p = buf;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static uint16_t tcp_checksum(const struct iphdr *ip, const struct tcphdr *tcp,
                             const uint8_t *payload, size_t payload_len) {
    struct pseudo { uint32_t saddr, daddr; uint8_t zero, proto; uint16_t len; } __attribute__((packed)) psh;
    psh.saddr = ip->saddr; psh.daddr = ip->daddr; psh.zero = 0; psh.proto = IPPROTO_TCP;
    psh.len = htons(sizeof(struct tcphdr) + payload_len);

    uint8_t buf[sizeof(psh) + sizeof(struct tcphdr) + 1500] = {0};
    size_t off = 0;
    memcpy(buf + off, &psh, sizeof(psh)); off += sizeof(psh);
    memcpy(buf + off, tcp, sizeof(struct tcphdr)); off += sizeof(struct tcphdr);
    if (payload_len) memcpy(buf + off, payload, payload_len), off += payload_len;
    return csum(buf, off);
}

int main() {
    const char *spoof_ip = "10.0.0.123";
    const char *dst_ip   = "127.0.0.1";
    const uint16_t src_port = 44444, dst_port = 8080;

    /* Ключевое: IPPROTO_TCP */
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) { perror("socket"); return 1; }

    int on = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        perror("IP_HDRINCL"); return 1;
    }

    uint8_t packet[IP_MAXPACKET] = {0};
    struct iphdr  *ip  = (struct iphdr  *)packet;
    struct tcphdr *tcp = (struct tcphdr *)(packet + sizeof(struct iphdr));

    ip->ihl = 5; ip->version = 4; ip->tos = 0;
    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    ip->id = htons(0x1337); ip->frag_off = 0; ip->ttl = 64;
    ip->protocol = IPPROTO_TCP;
    ip->saddr = inet_addr(spoof_ip);
    ip->daddr = inet_addr(dst_ip);
    ip->check = 0;

    tcp->source = htons(src_port); tcp->dest = htons(dst_port);
    tcp->seq = htonl(0xdeadbeef); tcp->ack_seq = 0;
    tcp->doff = sizeof(struct tcphdr) / 4; tcp->syn = 1;
    tcp->window = htons(65535); tcp->urg_ptr = 0; tcp->check = 0;

    ip->check  = csum(ip, sizeof(struct iphdr));
    tcp->check = tcp_checksum(ip, tcp, NULL, 0);

    struct sockaddr_in dst = { .sin_family = AF_INET, .sin_port = tcp->dest, .sin_addr.s_addr = ip->daddr };
    ssize_t sent = sendto(sock, packet, sizeof(struct iphdr)+sizeof(struct tcphdr), 0,
                          (struct sockaddr *)&dst, sizeof(dst));
    if (sent < 0) perror("sendto");
    else printf("SYN spoof %s:%u -> %s:%u sent (%zd bytes)\n",
                spoof_ip, src_port, dst_ip, dst_port, sent);
    close(sock);
    return 0;
}
