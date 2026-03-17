about  
- raw sockets and header spoofing: raw.c, spoof.c, udp_spoof.c  
- icmp echo: ping.c  
- tcp echo server: socket.c  
- tcp echo server with epoll: socket_epol.c  
- slide link: https://www.figma.com/deck/w6jlHtZmHcKBVlMISQquMb/sockets?node-id=1-42&t=EMqFNJZrGpotrLBl-1

```sh
gcc -o ping ping.c
gcc -o raw raw.c
gcc -o spoof spoof.c
gcc -o udp_spoof udp_spoof.c
gcc -o socket socket.c
gcc -o socket_epol socket_epol.c
```
