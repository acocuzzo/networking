#!/bin/bash
[[ -d build ]] || mkdir build
clang++ -g -std=c++17 -Wall -Werror -I. -o build/client  client.cpp -lgmock -lgtest -lgtest_main -lpthread
