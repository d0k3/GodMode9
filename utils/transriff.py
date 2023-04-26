#!/usr/bin/env python3

""" Create a TRF translation for GodMode9 from a translation JSON. """

import argparse
import json
import math
import pathlib
import struct
import sys


LANGUAGE_NAME = "GM9_LANGUAGE"


def ceil_to_multiple(num: int, base: int) -> int:
    """
    Return the ceiling of num which is a multiple of base.

    Args:
        num: Number whose ceiling to return.
        base: Value which num will become a multiple of.

    Returns:
        Num rounded to the next multiple of base.
    """
    return base * math.ceil(num / base)


def get_language(data: dict) -> bytes:
    """
    Get language name from JSON data.

    Args:
        data: JSON translation data.

    Returns:
        The translation's language name.

    Raises:
        ValueError: If no language name exists.
    """
    try:
        return data[LANGUAGE_NAME].encode("utf-8")
    except KeyError as exception:
        raise ValueError("invalid language data") from exception


def load_translations(data: dict) -> dict[str, bytearray]:
    """
    Load translations from JSON data.

    Args:
        data: JSON translation data.

    Returns:
        The loaded strings.
    """
    return {
            key: bytearray(value, "utf-8") + b"\0"
            for key, value in data.items()
            if key != LANGUAGE_NAME
    }


def strings_to_trf(mapping: dict[str, bytearray], version: int, language: str) -> bytearray:
    """
    Create a TRF file from translated strings.

    Args:
        mapping: Mapping between labels and translated strings.
        version: Translation version.
        language: Translation language.

    Returns:
        The translation strings as TRF data.
    """
    # File header.
    trfdata = bytearray(b"RIFF\0\0\0\0")

    # Metadata section.
    trfdata += (
            b"META"
            + struct.pack("<LLL32s", 40, version, len(mapping), language)
    )

    # String data section.
    data = bytearray().join(mapping.values())
    size = ceil_to_multiple(len(data), 4)
    padding = size - len(data)

    trfdata += (
            b"SDAT"
            + struct.pack("<L", size)
            + data
            + b"\0" * padding
    )

    # String map section.
    data = bytearray()
    offset = 0
    for item in mapping.values():
        data += struct.pack("<H", offset)
        offset += len(item)

    size = ceil_to_multiple(len(data), 4)
    padding = size - len(data)

    trfdata += (
            b"SMAP"
            + struct.pack("<L", size)
            + data
            + b"\0" * padding
    )

    # Fill in cumulative section size.
    trfdata[4:8] = struct.pack("<L", len(trfdata) - 8)

    return trfdata


def main(source: pathlib.Path, dest: pathlib.Path, version: int) -> None:
    """
    Entrypoint of transriff.

    Args:
        source: JSON to convert from.
        dest: TRF file to write.
        version: Translation version.
    """
    data = json.loads(source.read_text())

    try:
        language = get_language(data)
    except ValueError as exception:
        sys.exit(f"Fatal: {exception}.")
    mapping = load_translations(data)

    trf_file = strings_to_trf(mapping, version, language)

    dest.write_bytes(trf_file)
    print(f"Info: {dest.as_posix()} created with {len(mapping)} strings.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
            description="Create an TRF translation for GodMode9 from a translation JSON."
    )

    parser.add_argument(
            "source",
            type=pathlib.Path,
            help="JSON to convert from"
    )
    parser.add_argument(
            "dest",
            type=pathlib.Path,
            help="TRF file to write"
    )
    parser.add_argument(
            "version",
            type=int,
            help="translation version, from language.yml"
    )

    args = parser.parse_args()

    main(args.source, args.dest, args.version)
