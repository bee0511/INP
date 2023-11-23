#!/bin/sh
set -x
make
docker-compose restart
docker exec -it sw_tester /testcase/run.sh demo