set -e

if [ "$(id -u)" -ne 0 ]; then
  echo -e "\e[33mWARN: Running uninstallation script without sudo, may cause permission errors.\e[0m"
fi

echo -e "\e[33m[!]\e[0m Uninstalling monkype executable..."
rm $MONKYPE_INSTALL_DIR/monkype

echo -e "\e[33m[!]\e[0m Uninstalling scripts..."
rm $MONKYPE_INSTALL_DIR/monkype-*

echo -e "\e[34mFinished. Good bye!\e[0m"
