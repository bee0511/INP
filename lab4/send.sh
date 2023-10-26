#!/bin/sh

curl --silent 'inp.zoolab.org:10314/otp?name=110550164' > otp1.txt
curl -F 'file=@otp1.txt' 'inp.zoolab.org:10314/upload'