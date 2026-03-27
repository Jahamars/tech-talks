#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#define PORT 8080
#define MAX_EVENTS 10
int main() {
    int server_fd, client_fd, epoll_fd, nfds, opt = 1;
    struct sockaddr_in address;
    struct epoll_event ev, events[MAX_EVENTS];
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) exit(1);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("-1 bind ");
        exit(1);
    }
    listen(server_fd, 10);
    epoll_fd = epoll_create1(0);
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
    printf("port %d...\n", PORT);
    while (1) {
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == server_fd) {
                client_fd = accept(server_fd, NULL, NULL);
                if (client_fd < 0) continue;
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                
                printf("client %d\n", client_fd);
            } else {
                char buf[1024] = {0};
                int res = recv(events[n].data.fd, buf, sizeof(buf), 0);
                
                if (res <= 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[n].data.fd, NULL);
                    close(events[n].data.fd);
                    printf("client disconnected\n");
                } else {
                    printf("recv: %s", buf);
                    send(events[n].data.fd, buf, res, 0);
                }
            }
        }
    }
    return 0;
}
