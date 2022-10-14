#!/usr/bin/env python3

from argparse import ArgumentParser
from os import path

import json
import struct

parser = ArgumentParser(description="Creates an TRF translation for GodMode9 from a translation JSON")
parser.add_argument("input", type=str, help="JSON to convert from")
parser.add_argument("output", type=str, help="to output to")
parser.add_argument("version", type=int, help="translation version, from language.inl")

args = parser.parse_args()

with open(args.input, "r") as f:
    # read JSON
    strings = json.load(f)
    if "GM9_LANGUAGE" not in strings:
        print("Fatal: Input is not a valid JSON file")
        exit(1)

    # Encode strings to UTF-8 bytestrings
    strings = {item: strings[item].encode("utf-8") + b"\0" for item in strings}

    # Remove language name from strings
    lang_name = strings["GM9_LANGUAGE"]
    del strings["GM9_LANGUAGE"]

    # sort map
    # fontMap = sorted(fontMap, key=lambda x: x["mapping"])

    # write file
    with open(args.output, "wb") as out:
        out.write(b"RIFF")
        out.write(struct.pack("<L", 0))  # Filled at end

        # metadata
        out.write(b"META")
        out.write(struct.pack("<LLL32s", 40, args.version, len(strings), lang_name))

        # character data
        out.write(b"SDAT")
        sectionSize = sum(len(strings[item]) for item in strings)
        padding = 4 - sectionSize % 4 if sectionSize % 4 else 0
        out.write(struct.pack("<L", sectionSize + padding))
        out.write(b"".join(strings.values()))
        out.write(b"\0" * padding)

        # character map
        out.write(b"SMAP")
        sectionSize = len(strings) * 2
        padding = 4 - sectionSize % 4 if sectionSize % 4 else 0
        out.write(struct.pack("<L", sectionSize + padding))
        offset = 0
        for string in strings.values():
            out.write(struct.pack("<H", offset))
            offset += len(string)
        out.write(b"\0" * padding)

        # write final size
        outSize = out.tell()
        out.seek(4)
        out.write(struct.pack("<L", outSize - 8))

        print("Info: %s created with %d strings." % (args.output, len(strings)))
