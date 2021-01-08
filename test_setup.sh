#!/bin/bash

./build/peer 1     localhost 6500 21607 localhost 6502 10    localhost 6501&
./build/peer 10    localhost 6501 1     localhost 6500 21607 localhost 6502&
./build/peer 21607 localhost 6502 10    localhost 6501 1     localhost 6500&

cat test_value.txt | ./build/client localhost 6501 SET The\ ciiirrcle\ of\ Chord\!
killall peer
