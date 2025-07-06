#!/bin/bash

RAW_LINK="https://raw.githubusercontent.com/monkeytypegame/monkeytype/refs/heads/master/frontend/static/languages"


lang="$1"

while :; do
  if [ -z "$1" ]; then
    echo
    echo "Language files are available at:"
    echo "https://github.com/monkeytypegame/monkeytype/blob/master/frontend/static/languages/"
    echo
    echo "Enter the desired language name (omit the .json extension):"
    read lang
  fi

  echo "Downloading..."
  mkdir -p datasets/
  wget -c "$RAW_LINK/${lang}.json" -O "datasets/${lang}.json" >/dev/null 2>&1
  status=$?

  if [ $status -eq 0 ]; then
    ./convert.py "datasets/${lang}.json"
    break
  else
    echo "Invalid language name '${lang}', try again."
    if [ -n "$1" ]; then
      exit 1
    fi
  fi
done

