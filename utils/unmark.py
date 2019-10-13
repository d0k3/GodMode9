""" Create an user manual from GodMode9's README.md file. """
import argparse as _argparse
import gzip as _gzip
import json as _json
import os as _os
import re as _re
import sys as _sys


def _exit_fatal(msg):
    """ Print an error message to stderr and exit. """
    print("Fatal: {0}".format(msg), file=_sys.stderr)

    exit(1)


def _print_warn(msg):
    """ Print a warning to stderr. """
    print("Warning: {0}".format(msg, file=_sys.stderr))


def load_input(fname):
    """ Load a file. """
    with open(fname, "r") as f: # pylint: disable=invalid-name
        return f.read()


def load_patch(fname):
    """ Load a patch file. """
    with _gzip.open(fname, "rb") as f: # pylint: disable=invalid-name
        return _json.load(f)


def apply_patch(patch_data, data):
    """ Replace strings in data based on the records in patch_data. """
    unmatched = []
    for pattern in patch_data:
        # pattern[0]: original string, pattern[1]: replacement
        if data.find(pattern[0]) == -1:
            unmatched.append(pattern[0])
        else:
            data = data.replace(pattern[0], pattern[1])

    return (data, unmatched)


def strip_md(data):
    """ Remove certain MarkDown from data. """
    # NOTE: multiple occurrences of the same char go before single occurrences
    # ["__", "_"] ok, ["_", "__"] NOT ok
    blacklist = ["__", "**", "\\"]
    for pattern in blacklist:
        data = data.replace(pattern, "")

    return data


def write_output(data, fname, force):
    """ Write data to fname. """
    mode = "w" if force else "x"
    with open(fname, mode) as f: # pylint: disable=invalid-name
        f.write(data)


def replace_links(data):
    """ Replace links with their anchor text. """
    for link in _re.findall(r"\[(.*?)\]\((.*?)\)", data):
        pattern = "[{0}]({1})".format(link[0], link[1])

        data = data.replace(pattern, link[0])

    return data


def main(src, dst, force):
    """ Core function. """
    basedir = _os.path.dirname(_os.path.abspath(__file__))
    patch = _os.path.join(basedir, "patch.json.gz")

    try:
        data = load_input(src)
    except FileNotFoundError:
        _exit_fatal("{0}: no such file.".format(src))
    except PermissionError:
        _exit_fatal("{0}: permission denied.".format(src))
    except IsADirectoryError:
        _exit_fatal("{0}: is a directory.".format(src))

    patch_data = []
    try:
        patch_data = load_patch(patch)
    except FileNotFoundError:
        _print_warn("{0}: no such file.".format(patch))
    except PermissionError:
        _print_warn("{0}: pernission denied.".format(patch))
    except _json.decoder.JSONDecodeError:
        _print_warn("{0}: malformed JSON file.".format(patch))

    if patch_data:
        data, unmatched = apply_patch(patch_data, data)
        for pattern in unmatched:
            _print_warn("unmatched pattern: {0!r}".format(pattern))

    data = strip_md(data)
    data = replace_links(data)

    try:
        write_output(data, dst, force)
    except FileExistsError:
        _exit_fatal("{0}: file already exists. Pass -f to override.".format(dst))
    except PermissionError:
        _exit_fatal("{0}: permission denied.".format(dst))
    except IsADirectoryError:
        _exit_fatal("{0}: is a directory.".format(dst))


if __name__ == "__main__":
    AP = _argparse.ArgumentParser(
        description="Create an user manual from GodMode9's README.md file."
    )

    AP.add_argument(
        "src",
        type=str,
        help="the original README.md file"
    )
    AP.add_argument(
        "dst",
        type=str,
        help="the user manual"
    )
    AP.add_argument(
        "-f",
        "--force",
        default=False,
        action="store_true",
        help="overwrite the output file, if present"
    )

    ARGS = AP.parse_args()

    main(ARGS.src, ARGS.dst, ARGS.force)
