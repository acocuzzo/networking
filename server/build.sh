#!/bin/bash
[[ -d build ]] || mkdir build
clang++ -g -std=c++17 -Wall -Werror -I. -I../common -o build/server  server.cpp -lgmock -lgtest -lgtest_main -lpthread
