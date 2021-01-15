#!/bin/bash

./build/peer 10000 127.0.0.1 10000 16000 127.0.0.1 16000 12000 127.0.0.1 12000&
./build/peer 12000 127.0.0.1 12000 10000 127.0.0.1 10000 14000 127.0.0.1 14000&
./build/peer 14000 127.0.0.1 14000 12000 127.0.0.1 12000 16000 127.0.0.1 16000&
./build/peer 16000 127.0.0.1 16000 14000 127.0.0.1 14000 10000 127.0.0.1 10000&

echo "ding dong the witch is dead" | ./build/client localhost 14000 SET test
./build/client localhost 14000 GET test

killall peer
