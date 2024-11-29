#!/usr/bin/env python3

from argparse import ArgumentParser, FileType
import json

# This is to be incremented every time the order changes
# New strings added to the end will not cause issues
TRANSLATION_VER = 1

# This is the language name key in the JSON, which is to be deleted
LANGUAGE_NAME = "GM9_LANGUAGE"

parser = ArgumentParser(description="Creates the language.inl file from source.json")
parser.add_argument("source", type=FileType("r"), help="source.json")
parser.add_argument("inl", type=FileType("w"), help="language.inl")
args = parser.parse_args()

# Load the JSON and remove the language name
source = json.load(args.source)
del source[LANGUAGE_NAME]

# Create the header file
args.inl.write("#define TRANSLATION_VER %d\n\n" % TRANSLATION_VER)
for key in source:
    # Escape \r, \n, and quotes
    val = source[key].replace("\r", "\\r").replace("\n", "\\n").replace('"', '\\"')
    args.inl.write('STRING(%s, "%s")\n' % (key, val))
