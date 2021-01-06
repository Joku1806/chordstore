#!/bin/bash

./build/peer 0     localhost 6500 65535 localhost 6503 21845 localhost 6501&
./build/peer 21845 localhost 6501 0     localhost 6500 43690 localhost 6502&
./build/peer 43690 localhost 6502 21845 localhost 6501 65535 localhost 6503&
./build/peer 65535 localhost 6503 43690 localhost 6502 0     localhost 6500&

cat test_value.txt | ./build/client localhost 6500 SET who\ you\ gonna\ call\?
killall peer
