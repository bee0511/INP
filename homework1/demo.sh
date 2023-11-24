#!/bin/sh
set -x
make
docker-compose restart demo
docker exec -it sw_tester /testcase/run.sh demo