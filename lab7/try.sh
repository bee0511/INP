#!/bin/bash
set -ex

./compile.sh

python play.py ./bin/solver
