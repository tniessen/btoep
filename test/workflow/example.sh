#!/bin/bash
set -e

# Create a new dataset from the string "Hello world!"
echo Hello world! | btoep-add --dataset=hello --offset=0

# Display the dataset
data=$(btoep-read --dataset=hello)
if [ "$data" != "Hello world!" ]; then
  echo "Error: btoep-read returned unexpected result" && exit 1
fi

# Assume our dataset has a size of 900 bytes
btoep-set-size --dataset=hello --size=900

# Display how much data is missing
missing=$(btoep-list-ranges --dataset=hello --missing)
if [ "$missing" != "13..899" ]; then
  echo "Error: btoep-list-ranges returned unexpected result" && exit 1
fi

# Add more data somewhere in the middle of the dataset
echo SPACE | btoep-add --dataset=hello --offset=450

# List missing ranges again
missing=$(btoep-list-ranges --dataset=hello --missing)
if [ "$missing" != $'13..449\n456..899' ]; then
  echo "Error: btoep-list-ranges returned unexpected result" && exit 1
fi

# Try reading the file again
data=$(btoep-read --dataset=hello)
if [ "$data" != "Hello world!" ]; then
  echo "Error: btoep-read returned unexpected result" && exit 1
fi

# Find out where the second section is
end_of_first=$(btoep-find-offset --dataset=hello --start-at=0 --stop-at=no-data)
start_of_second=$(btoep-find-offset --dataset=hello --start-at=$end_of_first --stop-at=data)
if [ "$start_of_second" != 450 ]; then
  echo "Error: btoep-find-offset returned unexpected result" && exit 1
fi

# Read the second section
data=$(btoep-read --dataset=hello --offset=$start_of_second)
if [ "$data" != "SPACE" ]; then
  echo "Error: btoep-read returned unexpected result" && exit 1
fi
