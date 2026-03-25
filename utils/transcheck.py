#!/usr/bin/env python3

from argparse import ArgumentParser, FileType
import json

# Special keys
LANGUAGE_NAME = "GM9_LANGUAGE"
VERSION = "GM9_TRANS_VER"


def check_translation(source, translation, threshold):
    # Sanity check the versions
    if source[VERSION] != translation[VERSION]:
        raise Exception("Version mismatch (%d != %d)" % (source[VERSION], translation[VERSION]))
    del source[VERSION]

    # Make sure the language has a name
    if source[LANGUAGE_NAME] == translation[LANGUAGE_NAME]:
        print("\x1B[33mError: Language name matches source (%s)\x1B[39m" % translation[LANGUAGE_NAME])
        return False
    del source[LANGUAGE_NAME]

    # Check how many strings have been translated
    translated_count = 0
    for item in source:
        if item in translation and source[item] != translation[item]:
            translated_count += 1

    # Check if this translation meets the threshold percentage
    percent_translated = translated_count / len(source) * 100
    valid = percent_translated >= threshold
    print("\x1B[%om%s: %d of %d items translated (%.2f%%)\x1B[39m" % (0o32 if valid else 0o31, translation[LANGUAGE_NAME], translated_count, len(source), percent_translated))
    return valid


def main():
    parser = ArgumentParser(description="Checks if a translation is ready for use")
    parser.add_argument("source", type=FileType("r", encoding="utf-8"), help="source.json")
    parser.add_argument("translation", type=FileType("r", encoding="utf-8"), help="[LANG].json")
    parser.add_argument("threshold", type=int, help="minimum translation percentage")
    args = parser.parse_args()

    # Load the JSONs
    source = json.load(args.source)
    translation = json.load(args.translation)

    res = check_translation(source, translation, args.threshold)
    exit(0 if res else 1)


if __name__ == "__main__":
    main()
