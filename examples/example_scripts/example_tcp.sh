#! /bin/bash
# needs to be run from top-level directory, i.e. ./examples/example_scripts/example_local.sh
cd build/examples || exit 1

echo "Starting server..."
../rcat_tcp r &
SERVER_PID=$!
sleep 1

echo "Starting client..." 
echo "RastaTest" | ../rcat_tcp s &
CLIENT_PID=$!
sleep 1

echo "Waiting for client and server to quit..."
wait $CLIENT_PID && wait $SERVER_PID && exit 0
exit 1
