#!/usr/bin/env python3

from pathlib import Path
import json
import sys


def main():
    examples = Path(sys.argv[1])
    missing = []

    for configuration in sorted(examples.glob("*.json")):
        guide = configuration.with_suffix(".md")
        if not guide.is_file():
            missing.append(guide.name)
        try:
            json.loads(configuration.read_text())
        except (OSError, json.JSONDecodeError) as error:
            print(f"Invalid example JSON {configuration.name}: {error}")
            return 1

    if missing:
        print("Missing example guides: " + ", ".join(missing))
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
