#!/bin/bash
set -ex

./compile.sh

TOKEN=ffRop5Nq7E6gpbo3 python submit.py ./bin/server ./bin/client 
