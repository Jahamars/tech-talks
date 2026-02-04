#!/bin/bash
while true; do
    echo "$(date): Служба работает" >> /tmp/myservice.log
    sleep 10
done
