#!/bin/bash
set -e

# Create a new dataset and add content, one byte at a time.
btoep-create --dataset=random-writes --size=1024
for i in {0..1023}; do
  echo -n x | btoep-add --dataset=random-writes --offset=$i
done

# Ensure the index is okay.
ranges=$(btoep-list-ranges --dataset=random-writes)
if [ "$ranges" != "0..1023" ]; then
  echo "Error: index is incorrect" && exit 1
fi

# Ensure the data is okay.
data=$(btoep-read --dataset=random-writes)
correct_data=$(for i in {0..1023}; do echo -n x; done)
if [ "$correct_data" != "$data" ]; then
  echo "Error: stored data is incorrect" && exit 1
fi
