#!/bin/bash

#Chech whether no. of clients is passed as argument
if [ $# -ne 1 ]; then
  echo "No Argument Is Passed Exiting"
  exit 1
fi

# Define the number of client instances to run
NUM_CLIENTS=$1

# Array to store process IDs (PIDs) of client instances
client_pids=()

# signal handler function
kill_clients() {
    echo "Elapsed time: $elapsed_time seconds"
    echo "Killing all client processes..."
    for pid in "${client_pids[@]}"; do
        kill "$pid"
    done
    exit 1
}

# Trap to handle interrupt signal
trap kill_clients SIGINT

# Start time
start_time=$(date +%s.%N)

# To Run N no. of client code simultaneously 
for ((i=1; i<=$NUM_CLIENTS; i++))
do
    # Run client code in the background and store its PID
    ./client "$i" &
    client_pids+=("$!")
done

# End time
end_time=$(date +%s.%N)

elapsed_time=$(echo "$end_time - $start_time" | bc)

echo "Elapsed time: $elapsed_time seconds"

# Wait for all client instances to finish
wait


echo "All client instances have finished."
