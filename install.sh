#!/bin/bash

set -e

if [ "$(id -u)" -ne 0 ]; then
  echo -e "\e[33mWARN: Running installation script without sudo, may cause permission errors.\e[0m"
fi

echo "[ Installing monkype ]"

echo -e "\e[32m*\e[0m Building executable..."
make clean >/dev/null
make >/dev/null

cp ./monkype /usr/local/bin
echo -e "\e[32m*\e[0m Copying \e[34m'mokype'\e[0m to /usr/local/bin..."

echo -e "\e[32m*\e[0m Copying utility scripts to /usr/local/bin..."
for i in $(ls scripts); do
  echo -e "  \e[32m*\e[0m Copying \e[34m'mokype-$i'\e[0m..."
  cp ./scripts/$i /usr/local/bin/monkype-$i
done

echo -e "\e[32m*\e[0m Finished."

echo
echo -e "\e[34m[?]\e[0m Use \e[32mmokype-list\e[0m to see available datasets"
echo -e "\e[34m[?]\e[0m Run \e[32mmokype-get <dataset>\e[0m to install and use datasets"
echo -e "\e[34m[?]\e[0m Uninstall with \e[31mmokype-uninstall\e[0m"
