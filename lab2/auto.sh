#!/bin/bash

# Step 1: Download the challenge.bin file
curl -o challenge.bin "https://inp.zoolab.org/binflag/challenge?id=110550164"

gcc -o demo lab2.c
# Step 2: Run the demo program with the downloaded file
./demo challenge.bin > flag.txt

# Step 3: Extract the flag from the output
flag=$(cat flag.txt | tr -d '\n')
echo flag: $flag
# Step 4: Send the flag to the verification URL
curl "https://inp.zoolab.org/binflag/verify?v=$flag"

# Cleanup: Remove temporary files
rm challenge.bin flag.txt
