#!/bin/bash
[[ -d build ]] || mkdir build
clang++ -g -std=c++17 -Wall -Werror -I. -o build/server  server.cpp -lgmock -lgtest -lgtest_main -lpthread
