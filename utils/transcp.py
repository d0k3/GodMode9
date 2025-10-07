#!/usr/bin/env python3

from argparse import ArgumentParser, FileType
import json

# Special keys
LANGUAGE_NAME = "GM9_LANGUAGE"
VERSION = "GM9_TRANS_VER"

parser = ArgumentParser(description="Creates the language.inl file from source.json")
parser.add_argument("source", type=FileType("r", encoding="utf-8"), help="source.json")
parser.add_argument("inl", type=FileType("w", encoding="utf-8"), help="language.inl")
args = parser.parse_args()

# Load the JSON and handle the meta values
source = json.load(args.source)
version = source[VERSION]
del source[VERSION]
del source[LANGUAGE_NAME]

# Create the header file
args.inl.write("#define TRANSLATION_VER %d\n\n" % version)
for key in source:
    # Escape \r, \n, and quotes
    val = source[key].replace("\r", "\\r").replace("\n", "\\n").replace('"', '\\"')
    args.inl.write('STRING(%s, "%s")\n' % (key, val))
