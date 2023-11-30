#!/bin/sh
# Token=Q6g5P3f8B3dJ4tgp
g++ -static -Ofast -g -o bin/server server.cpp
g++ -static -Ofast -g -o bin/client client.cpp
python3 submit.py bin/server bin/client