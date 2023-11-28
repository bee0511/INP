#!/bin/bash
set -x

g++ -static -o bin/server src/server.cpp
g++ -static -o bin/client src/client.cpp