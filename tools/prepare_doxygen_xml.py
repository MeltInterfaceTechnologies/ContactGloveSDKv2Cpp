"""Keep Doxygen initializer text readable by moxygen's plain-text parser.

Doxygen wraps documented enum values in <ref> elements. moxygen reads only the
initializer's direct text and otherwise renders an incomplete `field =` line.
Flatten markup inside initializers in the build XML, preserving the text in
order. The public header remains the source of the documented values.
"""

from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def prepare(directory):
    changed = 0
    for path in sorted(Path(directory).glob("*.xml")):
        tree = ET.parse(path)
        modified = False
        for initializer in tree.iter("initializer"):
            if len(initializer):
                text = "".join(initializer.itertext())
                for child in list(initializer):
                    initializer.remove(child)
                initializer.text = text
                modified = True
        if modified:
            tree.write(path, encoding="utf-8", xml_declaration=True)
            changed += 1
    return changed


if __name__ == "__main__":
    print(f"Prepared initializer text in {prepare(sys.argv[1])} XML files")
