#!/bin/python3

# This is script is intent to convert monkeytype language json files to a simple words.txt with single word per line


import sys
import json

with open("./words.txt", "w") as words:
    if len(sys.argv) < 2:
        print("You need to specify path to a input file!")
        exit()

    input = sys.argv[1]
    with open(input, "r") as jsonFile:
        data = json.load(jsonFile)

        print("Converting %s..." % (data["name"]))

        max_len = 0
        for word in data["words"]:
            words.write(word + "\n")
            max_len = max(max_len, len(word))

        print("Max word length: %d" % (max_len))
