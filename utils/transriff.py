#!/usr/bin/env python3

""" Create an TRF translation for GodMode9 from a translation JSON. """

from __future__ import annotations

import argparse
import dataclasses
import json
import math
import pathlib
import struct
import sys


LANGUAGE_NAME = "GM9_LANGUAGE"


def read_args() -> argparse.Namespace:
    """
    Parse command-line args.

    Returns:
        The parsed command-line args.
    """
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

    return parser.parse_args()


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
    except AttributeError as exception:
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


@dataclasses.dataclass
class TRFMetadata:
    """
    A TRF file's metadata section.

    Args:
        version: Translation version.
        nstrings: Total strings in the translation.
        language: Translation language.
    """
    version: int
    nstrings: int
    language: bytes

    def as_bytearray(self) -> bytearray:
        """
        Return a bytearray representation of this TRF section.

        Returns:
            The TRF metadata section as a bytearray.
        """
        return (
                bytearray(b"META")
                + struct.pack("<LLL32s", 40, self.version, self.nstrings, self.language)
        )

    def __len__(self) -> int:
        return len(self.as_bytearray())


@dataclasses.dataclass
class TRFCharacterData:
    """
    A TRF file's character data section.

    Args:
        data: Translation strings.
    """
    data: bytearray

    @classmethod
    def from_mapping(cls, mapping: dict[str, bytearray]) -> TRFCharacterData:
        """
        Construct an instance of this class from a translation mapping.

        Args:
            mapping: Mapping between translation labels and strings.

        Returns:
            An instance of TRFCharacterData.
        """
        return cls(bytearray().join(mapping.values()))

    def as_bytearray(self) -> bytearray:
        """
        Return a bytearray representation of this TRF section.

        Returns:
            This TRF character data section as a bytearray.
        """
        size = ceil_to_multiple(len(self.data), 4)
        padding = size - len(self.data)

        return (
                bytearray(b"SDAT")
                + struct.pack("<L", size)
                + self.data
                + b"\0" * padding
        )

    def __len__(self) -> int:
        return len(self.as_bytearray())


@dataclasses.dataclass
class TRFCharacterMap:
    """
    A TRF file's character map section.

    Args:
        data: Translation strings' offsets.
    """
    data: bytearray

    @classmethod
    def from_mapping(cls, mapping: dict[str, bytearray]) -> TRFCharacterMap:
        """
        Construct an instance of this class from a translation mapping.

        Args:
            mapping: Mapping between translation labels and strings.

        Returns:
            An instance of TRFCharacterMap.
        """
        data = bytearray()
        offset = 0

        for item in mapping.values():
            data.extend(struct.pack("<H", offset))
            offset += len(item)

        return cls(data)

    def as_bytearray(self) -> bytearray:
        """
        Return a bytearray representation of this TRF section.

        Returns:
            This TRF character map section as a bytearray.
        """
        size = ceil_to_multiple(len(self.data), 4)
        padding = size - len(self.data)

        return (
                bytearray(b"SMAP")
                + struct.pack("<L", size)
                + self.data
                + b"\0" * padding
        )

    def __len__(self) -> int:
        return len(self.as_bytearray())


@dataclasses.dataclass
class TRFFile:
    """
    A TRF file.

    Args:
        metadata: The TRF META section.
        chardata: The TRF SDAT section.
        charmap: The TRF SMAP section.
    """
    metadata: TRFMetadata
    chardata: TRFCharacterData
    charmap: TRFCharacterMap

    @classmethod
    def new(cls, version: int, mapping: dict, language: bytes) -> TRFFile:
        """
        Construct an instance of this class and its attributes.

        Args:
            version: Translation version.
            mapping: Mapping between translation labels and strings.
            language: Translation language.

        Returns:
            An instance of TRFFile.
        """
        return cls(
                TRFMetadata(version, len(mapping), language),
                TRFCharacterData.from_mapping(mapping),
                TRFCharacterMap.from_mapping(mapping)
        )

    def as_bytearray(self) -> bytearray:
        """
        Return a bytearray representation of this TRF file.

        Returns:
            This TRF file as a bytearray.
        """
        size = len(self.metadata) + len(self.chardata) + len(self.charmap)

        return (
                bytearray(b"RIFF")
                + struct.pack("<L", size)
                + self.metadata.as_bytearray()
                + self.chardata.as_bytearray()
                + self.charmap.as_bytearray()
        )


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
    strings = load_translations(data)

    trf_file = TRFFile.new(version, strings, language)

    dest.write_bytes(trf_file.as_bytearray())
    print(f"Info: {dest.as_posix()} created with {len(strings)} strings.")


if __name__ == "__main__":
    args = read_args()
    main(args.source, args.dest, args.version)
