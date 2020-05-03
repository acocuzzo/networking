cd client
./build.sh
cd ../server
./build.sh
cd ..
./server/build/server &
./client/build/client localhost sample.txt A &
./client/build/client localhost sample.txt B
killall server
