# linux system programming

Работа с системными вызовами Linux на C: файловый ввод/вывод, процессы, демонизация.

## файлы

| файл | что делает |
|------|-----------|
| `read.c` | читает `data.txt` через `open()` / `read()` / `close()` |
| `write.c` | создаёт `data.txt` через `open(O_CREAT\|O_WRONLY)` / `write()` |
| `makeproc.c` | выводит PID и PPID текущего процесса |
| `deamon/deamon.c` | демон: `fork()` → `setsid()` → цикл с логом в `/tmp/daemon.log` |
| `deamon/myservice.service` | systemd unit для bash-скрипта |
| `deamon/myservice.sh` | пишет метку времени в `/tmp/myservice.log` каждые 10 сек |

## сборка
```bash
gcc read.c -o read
gcc write.c -o write
gcc makeproc.c -o proc
gcc deamon/deamon.c -o deamon/daemon
```

## запуск демона 
```bash
./deamon/daemon
cat /tmp/daemon.log
kill $(cat /tmp/daemon.pid)
```

## запуск через systemd
```bash
sudo cp deamon/myservice.service /etc/systemd/system/
sudo cp deamon/myservice.sh /opt/
sudo chmod +x /opt/myservice.sh
sudo systemctl daemon-reload
sudo systemctl start myservice
journalctl -u myservice -f
```

## strace
```bash
strace ./read
strace -e write ./write
strace -e execve,fork,setsid ./deamon/daemon
```
