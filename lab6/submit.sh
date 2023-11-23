#!/bin/bash
set -ex

g++ -static -o ./src/server ./src/server.cpp
g++ -static -o ./src/client ./src/client.cpp

python submit.py ./src/server ./src/client 
