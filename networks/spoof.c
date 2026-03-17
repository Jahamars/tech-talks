#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define TARGET_IP   "127.0.0.1"
#define FAKE_SRC_IP "1.2.3.4"      // поддельный src
#define TARGET_PORT 8080

// Буфер для пакета
char packet[4096];

// Подсчёт контрольной суммы (стандартный)
unsigned short checksum(void *data, int len) {
    unsigned short *buf = data;
    unsigned int sum = 0;
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len) sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

// TCP checksum требует pseudo-header
unsigned short tcp_checksum(struct iphdr *iph, struct tcphdr *tcph) {
    struct {
        uint32_t src, dst;
        uint8_t  zero;
        uint8_t  proto;
        uint16_t tcp_len;
    } pseudo;

    pseudo.src     = iph->saddr;
    pseudo.dst     = iph->daddr;
    pseudo.zero    = 0;
    pseudo.proto   = IPPROTO_TCP;
    pseudo.tcp_len = htons(sizeof(struct tcphdr));

    char buf[sizeof(pseudo) + sizeof(struct tcphdr)];
    memcpy(buf, &pseudo, sizeof(pseudo));
    memcpy(buf + sizeof(pseudo), tcph, sizeof(struct tcphdr));

    return checksum(buf, sizeof(buf));
}

int main() {
    // Raw socket — нужен root / CAP_NET_RAW
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("socket (нужен root)");
        return 1;
    }

    // Говорим ядру, что IP-заголовок строим сами
    int one = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    memset(packet, 0, sizeof(packet));

    // --- IP header ---
    struct iphdr *iph = (struct iphdr *)packet;
    iph->ihl      = 5;
    iph->version  = 4;
    iph->tos      = 0;
    iph->tot_len  = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph->id       = htons(54321);
    iph->frag_off = 0;
    iph->ttl      = 64;
    iph->protocol = IPPROTO_TCP;
    iph->saddr    = inet_addr(FAKE_SRC_IP);   // <-- подделка
    iph->daddr    = inet_addr(TARGET_IP);
    iph->check    = checksum(iph, sizeof(struct iphdr));

    // --- TCP header ---
    struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct iphdr));
    tcph->source  = htons(12345);
    tcph->dest    = htons(TARGET_PORT);
    tcph->seq     = htonl(0xdeadbeef);
    tcph->ack_seq = 0;
    tcph->doff    = 5;          // data offset: 5 * 4 = 20 байт
    tcph->syn     = 1;          // SYN флаг
    tcph->window  = htons(65535);
    tcph->check   = tcp_checksum(iph, tcph);

    // --- Отправка ---
    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port   = htons(TARGET_PORT),
        .sin_addr.s_addr = inet_addr(TARGET_IP)
    };

    if (sendto(sock, packet, ntohs(iph->tot_len), 0,
               (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        perror("sendto");
    } else {
        printf("SYN отправлен: src=%s → %s:%d\n",
               FAKE_SRC_IP, TARGET_IP, TARGET_PORT);
    }

    close(sock);
    return 0;
}
