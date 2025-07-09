#!/bin/bash

set -e

echo "Building executable..."
make clean >/dev/null
make >/dev/null

cp ./monkype /usr/local/bin
echo -e "Copying \e[34mmokype\e[0m to /usr/local/bin..."

for i in $(ls scripts); do
  echo -e "Copying \e[34mmokype-$i\e[0m to /usr/local/bin..."
  cp ./scripts/$i /usr/local/bin/monkype-$i
done

echo Finished.

echo
echo -e "Run \e[32mmokype-get\e[0m to install datasets!"
