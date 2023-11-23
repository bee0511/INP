#!/bin/bash
set -ex

g++ -static -o ./bin/server ./src/server.cpp
g++ -static -o ./bin/client ./src/client.cpp

python submit.py ./bin/server ./bin/client 
