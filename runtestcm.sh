#!/bin/bash

for i in {10000..10009}
do
    ./Server 127.0.0.1 $i CM info_test.txt &
done
