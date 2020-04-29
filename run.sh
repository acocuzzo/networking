cd client
./build.sh
cd ../server
./build.sh
cd ..
./server/build/server &
gdb -q --args "./client/build/client" localhost "sample.txt"
killall server
