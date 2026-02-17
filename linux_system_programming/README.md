# linux system programming

## структура
```
├── read.c        # чтение файла через syscall
├── write.c       # запись файла через syscall
├── makeproc.c    # получение PID / PPID
├── data.txt      
└── deamon/
    ├── deamon.c           # демон-процесс
    ├── myservice.service  # systemd unit
    └── myservice.sh       # скрипт для логирование 
```

## сборка
```bash
gcc read.c -o read
gcc write.c -o write
gcc makeproc.c -o proc
gcc deamon/deamon.c -o deamon/daemon
```

## strace
```bash
strace ./read
strace -e write ./write
```

## systemd unit

```bash
systemctl daemon-reload
systemctl start myservice
systemctl status myservice
journalctl -u myservice -f
```
