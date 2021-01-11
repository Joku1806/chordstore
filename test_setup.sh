#!/bin/bash

./build/peer 1000  localhost 6500 53428 localhost 6504 14107 localhost 6501&
./build/peer 14107 localhost 6501 1000  localhost 6500 27214 localhost 6502&
./build/peer 27214 localhost 6502 14107 localhost 6501 40321 localhost 6503&
./build/peer 40321 localhost 6503 27214 localhost 6502 53428 localhost 6504&
./build/peer 53428 localhost 6504 40321 localhost 6503 1000  localhost 6500&

echo "100% !!11einself!" | ./build/client localhost 6500 SET My\ C-Programming-Skillz
./build/client localhost 6504 DELETE My\ C-Programming-Skillz
echo "sendto()sendto()sendto()sendto()" | ./build/client localhost 6501 SET My\ C-Programming-Skillz
./build/client localhost 6500 GET My\ C-Programming-Skillz

killall peer
