#!/usr/bin/env python3

import argparse
import tarfile
import glob
import os.path
import posixpath
from os import unlink

# don't add useless files
prefix_to_ignore = ('thumbs.db', 'desktop.ini', '$recycle.bin', '.')


class PathTooLongException(Exception):
    """Filename is too long to be added to the TAR file."""


class TarTooLargeException(Exception):
    """Resulting tar is larger than the given size."""


def tarpack(*, items, out, size_limit=0, path_limit=0, make_new=False):
    with tarfile.open(out, 'w' if make_new else 'a', format=tarfile.USTAR_FORMAT, bufsize=tarfile.BLOCKSIZE) as tar:
        def addtotar(realpath, tarpath):
            if path_limit and len(tarpath) > path_limit:
                raise PathTooLongException("path is longer than {} chars ({}): {}".format(path_limit, len(tarpath), tarpath))
            print('add:', tarpath)
            info = tarfile.TarInfo(tarpath)
            info.size = os.path.getsize(realpath)
            with open(realpath, 'rb') as f:
                tar.addfile(tarinfo=info, fileobj=f)

        def iterdir(realpath, tarpath):
            items = os.listdir(realpath)
            if path_limit and len(tarpath) > path_limit:
                raise PathTooLongException("path is longer than {} chars ({}): {}".format(path_limit, len(tarpath), tarpath))
            info = tarfile.TarInfo(tarpath)
            info.type = tarfile.DIRTYPE
            tar.addfile(info)
            for path in items:
                new_realpath = os.path.join(realpath, path)
                if os.path.isdir(new_realpath) and not os.path.basename(path).lower().startswith(prefix_to_ignore):
                    iterdir(new_realpath, posixpath.join(tarpath, path))
                elif os.path.isfile(new_realpath) and not os.path.basename(path).lower().startswith(prefix_to_ignore):
                    addtotar(os.path.join(realpath, path), posixpath.join(tarpath, path))

        for i in items:
            if os.path.isdir(i):
                iterdir(i, os.path.basename(i))
            elif os.path.isfile(i):
                addtotar(i, os.path.basename(i))
            else:
                raise FileNotFoundError("couldn't find " + i)

        # tarfile is adding more end blocks when it only needs two
        tar.fileobj.seek(0, 2)
        tar.fileobj.write(tarfile.NUL * (tarfile.BLOCKSIZE * 2))
        tar.fileobj.close()
        tar.closed = True

    tarsize = os.path.getsize(out)
    if size_limit and tarsize > size_limit:
        raise TarTooLargeException("TAR size is {} bytes is larger than the limit of {} bytes".format(tarsize, size_limit))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Pack files into a TAR file, leaving out extra information.")
    parser.add_argument('--make-new', '-n', help="Always create a new TAR file.", action='store_true')
    parser.add_argument('--size-limit', '-l', type=int, help="Throw an error when the file size reaches the specified limit.")
    parser.add_argument('--path-limit', '-p', type=int, help="Throw an error when a file path is longer than the specified limit.")
    parser.add_argument('out', help="Output filename.")
    parser.add_argument('items', nargs='+', help="Files and directories to add.")

    a = parser.parse_args()
    tarpack(items=a.items, out=a.out, size_limit=a.size_limit, path_limit=a.path_limit, make_new=a.make_new)
