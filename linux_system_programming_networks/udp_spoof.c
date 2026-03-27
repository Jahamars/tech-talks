#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
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

static uint16_t udp_checksum(const struct iphdr *ip,
                             const struct udphdr *udp,
                             const uint8_t *payload, size_t plen) {
    struct pseudo { uint32_t saddr,daddr; uint8_t zero,proto; uint16_t len; } __attribute__((packed)) psh;
    psh.saddr = ip->saddr; psh.daddr = ip->daddr; psh.zero = 0; psh.proto = IPPROTO_UDP;
    psh.len = htons(sizeof(struct udphdr) + plen);

    uint8_t buf[sizeof(psh) + sizeof(struct udphdr) + 1500] = {0};
    size_t off = 0;
    memcpy(buf+off, &psh, sizeof(psh)); off += sizeof(psh);
    memcpy(buf+off, udp, sizeof(struct udphdr)); off += sizeof(struct udphdr);
    if (plen) memcpy(buf+off, payload, plen), off += plen;

    return csum(buf, off);
}

int main() {
    const char *spoof_ip = "1.2.3.4";          // подменённый источник
    const char *dst_ip   = "192.168.0.10";     // <-- замените на свой реальный IP
    const uint16_t src_port = 55555;
    const uint16_t dst_port = 8080;
    const char payload[] = "hello via spoofed UDP";

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock < 0) { perror("socket"); return 1; }

    int on = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        perror("IP_HDRINCL"); return 1;
    }

    uint8_t packet[IP_MAXPACKET] = {0};
    struct iphdr  *ip  = (struct iphdr  *)packet;
    struct udphdr *udp = (struct udphdr *)(packet + sizeof(struct iphdr));
    uint8_t *data = packet + sizeof(struct iphdr) + sizeof(struct udphdr);
    size_t plen = strlen(payload);

    memcpy(data, payload, plen);

    /* IP header */
    ip->ihl = 5; ip->version = 4; ip->tos = 0;
    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + plen);
    ip->id = htons(0x1234); ip->frag_off = 0; ip->ttl = 64;
    ip->protocol = IPPROTO_UDP;
    ip->saddr = inet_addr(spoof_ip);
    ip->daddr = inet_addr(dst_ip);
    ip->check = 0;
    ip->check = csum(ip, sizeof(struct iphdr));

    /* UDP header */
    udp->source = htons(src_port);
    udp->dest   = htons(dst_port);
    udp->len    = htons(sizeof(struct udphdr) + plen);
    udp->check  = 0;
    udp->check  = udp_checksum(ip, udp, data, plen);

    struct sockaddr_in dst = { .sin_family = AF_INET,
                               .sin_port = udp->dest,
                               .sin_addr.s_addr = ip->daddr };

    ssize_t sent = sendto(sock, packet,
                          sizeof(struct iphdr)+sizeof(struct udphdr)+plen,
                          0, (struct sockaddr *)&dst, sizeof(dst));
    if (sent < 0) perror("sendto");
    else printf("UDP spoof %s:%u -> %s:%u sent (%zd bytes)\n",
                spoof_ip, src_port, dst_ip, dst_port, sent);

    close(sock);
    return 0;
}
