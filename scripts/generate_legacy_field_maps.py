#!/usr/bin/env python

import os
import re
import sys

HEADER_STRING = ("*" * 72)

SCRIPT_DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
ROTOBOX_ROOT = os.path.dirname(SCRIPT_DIR)
DOCS_DIR = os.path.join(ROTOBOX_ROOT, "docs")

APT_LAYOUT_FILEPATH = os.path.join(DOCS_DIR, "apt_rf.txt")


regex = re.compile("(?:[LR][ ]*[AN]{1,2}[ ]*)(?P<length>[0-9]*)(?:[ ]*)(?P<start>[0-9]*)(?:[ ]*[a-zA-Z0-9/]*[ ]*)(?P<name>[\"a-zA-Z0-9 \-\(\)\']*)")

with open(APT_LAYOUT_FILEPATH) as fHandle:
    raw_txt = fHandle.readlines()

for line in raw_txt:
    if(HEADER_STRING in line):
        print HEADER_STRING

    result = re.match(regex, line)
    if(result is not None):
        name = result.group('name').strip()
        length = int(result.group('length'))
        startPos = int(result.group('start')) - 1

        print "(\"{0}\", {1}, {2}),".format(name, startPos, startPos + length)

    # For debug, to print out all the lines not caught by the regex.
    #else:
    #    print line

