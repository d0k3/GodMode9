#!/usr/bin/env python3

from argparse import ArgumentParser
import struct
from os import path

parser = ArgumentParser(description="Creates an FRF font for GodMode9 from a PBM image")
parser.add_argument("input", type=str, help="PBM image to convert from")
parser.add_argument("output", type=str, help="out.to output to")
parser.add_argument("width", type=int, help="character width")
parser.add_argument("height", type=int, help="character height")
parser.add_argument("-m", "--map", metavar="map.txt", type=str, help="character map (whitespace separated Unicode codepoints)")

args = parser.parse_args()

if(args.width > 8):
    print("Fatal: Font width too large (Maximum is 8)")
    exit(1)

if(args.height > 10):
    print("Fatal: Font height too large (Maximum is 10)")
    exit(1)

with open(args.input, "rb") as f:
    # read PBM
    pbm = f.read()
    split = pbm.split(b"\n")
    if split[0] != b"P4":
        print("Fatal: Input is not a PBM file")
        exit(1)

    # skip comments
    for i in range(1, len(split)):
        if split[i][0] != ord("#"):
            imgWidth = int(split[i].split()[0])
            imgHeight = int(split[i].split()[1])
            imgData = b"\n".join(split[i + 1:])
            break

    count = imgWidth * imgHeight // args.width // args.height
    columns = imgWidth // args.width
    fontMap = []

    # prepare map
    mapPath = args.map
    if not mapPath and path.exists(args.input[:args.input.rfind(".")] + ".txt"):
        mapPath = args.input[:args.input.rfind(".")] + ".txt"
        print("Info: Using %s for font mappings" % mapPath)
    if mapPath:
        with open(mapPath, "r") as fontMapFile:
            fontMapTemp = fontMapFile.read().split()
            if (len(fontMapTemp) > count):
                print("Fatal: Font map has more items than possible in image (%d items in map)" % count)
                exit(1)
            elif len(fontMapTemp) < count:
                count = len(fontMapTemp)
                print("Info: Font map has fewer items than possible in image, only using first %d" % count)

            for item in fontMapTemp:
                fontMap.append({"mapping": int(item, 16)})
    else:
        print("Warning: Font mapping not found, mapping directly to Unicode codepoints")
        for i in range(count):
            fontMap.append({"mapping": i})

    # add unsorted tiles to map
    for c in range(count):
        fontMap[c]["bitmap"] = []
        for row in range(args.height):
            ofs = ((c // columns * args.height + row) * (imgWidth + ((8 - (imgWidth % 8)) if imgWidth % 8 != 0 else 0)) // 8)
            bp0 = ((c % columns) * args.width) >> 3
            bm0 = ((c % columns) * args.width) % 8
            byte = (((imgData[ofs + bp0] << bm0) | ((imgData[ofs + bp0 + 1] >> (8 - bm0)) if ofs + bp0 + 1 < len(imgData) else 0)) & (0xFF << (8 - args.width))) & 0xFF
            fontMap[c]["bitmap"].append(byte)

    # remove duplicates
    fontMap = list({x["mapping"]: x for x in fontMap}.values())
    if len(fontMap) != count:
        print("Info: %d duplicate mappings were removed" % (count - len(fontMap)))
        count = len(fontMap)

    # sort map
    fontMap = sorted(fontMap, key=lambda x: x["mapping"])

    # write file
    with open(args.output, "wb") as out:
        out.write(b"RIFF")
        out.write(struct.pack("<L", 0))  # Filled at end

        # metadata
        out.write(b"META")
        out.write(struct.pack("<LBBH", 4, args.width, args.height, count))

        # character data
        out.write(b"CDAT")
        sectionSize = count * args.height
        padding = 4 - sectionSize % 4 if sectionSize % 4 else 0
        out.write(struct.pack("<L", sectionSize + padding))
        for item in fontMap:
            for px in item["bitmap"]:
                out.write(struct.pack("B", px))
        out.write(b"\0" * padding)

        # character map
        out.write(b"CMAP")
        sectionSize = count * 2
        padding = 4 - sectionSize % 4 if sectionSize % 4 else 0
        out.write(struct.pack("<L", sectionSize + padding))
        for item in fontMap:
            out.write(struct.pack("<H", item["mapping"]))
        out.write(b"\0" * padding)

        # write final size
        outSize = out.tell()
        out.seek(4)
        out.write(struct.pack("<L", outSize - 8))

        print("Info: %s created with %d tiles." % (args.output, count))
