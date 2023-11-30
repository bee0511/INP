#!/bin/sh
# set -x
make builder
docker-compose restart demo
# docker exec -it sw_tester /testcase/run_hidden.sh demo

for i in $(seq 100); do
    sleep 5
    docker-compose restart demo
done