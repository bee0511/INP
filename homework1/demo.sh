#!/bin/sh
# set -x
make builder
docker-compose restart demo
docker exec -it sw_tester /testcase/run.sh demo

for i in $(seq 10); do
    sleep 10
    docker-compose restart demo
done