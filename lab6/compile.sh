#!/bin/bash
set -x

g++ -static -o server server.cpp
g++ -static -o client client.cpp