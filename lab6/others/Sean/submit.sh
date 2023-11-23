#!/bin/bash
set -ex
make
python submit.py ./bin/server ./bin/client 
