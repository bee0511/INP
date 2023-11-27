#!/bin/sh
set -x
make builder
docker-compose restart demo
docker exec -it sw_tester /testcase/run.sh demo