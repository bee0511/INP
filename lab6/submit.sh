#!/bin/bash
set -ex

./compile.sh

python submit.py ./bin/server ./bin/client 
