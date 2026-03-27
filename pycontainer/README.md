# pycontainer

Минимальный контейнер на Python через Linux namespaces и cgroups 

## как работает

1. Создаёт cgroup в `/sys/fs/cgroup/<name>` — выставляет лимиты памяти и CPU
2. Собирает rootfs во временной директории: копирует BusyBox, bash + зависимости через `ldd`
3. Запускает процесс через `unshare` с изоляцией PID/NET/MNT/UTS/IPC + `chroot`

## namespaces и cgroups
```
Namespaces : PID, NET, MNT, UTS, IPC
CGroups v2 : memory.max, cpu.max
RootFS     : BusyBox static + bash с либами
```

## требования
```bash
apt install busybox-static
```

## запуск
```bash
sudo python3 main.py shell    # интерактивный sh
sudo python3 main.py run      # echo + ls внутри контейнера
sudo python3 main.py memory   # тест OOM killer (20MB лимит, пытается выделить 100MB)
```

## что видно внутри
```
/ # ps aux        # только свои процессы (PID namespace)
/ # ip a          # пустой сетевой стек (NET namespace)
/ # hostname      # изолированный UTS
```
