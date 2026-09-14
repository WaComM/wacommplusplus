#!/usr/bin/env python3

from pathlib import Path
import json
import sys


def main():
    examples = Path(sys.argv[1])
    missing = []

    for configuration in sorted(examples.glob("*/*.json")):
        guide = configuration.parent / "README.md"
        if not guide.is_file():
            guide = configuration.parent / "docs" / "README.md"
        if not (configuration.parent / "docs" / "figures").is_dir():
            print(f"Missing docs/figures directory in {configuration.parent.name}")
            return 1
        if not guide.is_file():
            missing.append(str(guide.relative_to(examples)))
        else:
            text = guide.read_text()
            if "performance-evaluation.md" not in text:
                print(f"Missing shared performance protocol in {guide.name}")
                return 1
            if "## References" not in text:
                print(f"Missing References section in {guide.name}")
                return 1
            if "doi:" not in text.lower():
                print(f"Missing persistent peer-reviewed citation in {guide.name}")
                return 1
        try:
            json.loads(configuration.read_text())
        except (OSError, json.JSONDecodeError) as error:
            print(f"Invalid example JSON {configuration.name}: {error}")
            return 1

    if missing:
        print("Missing example guides: " + ", ".join(missing))
        return 1

    sarno = examples / "wacomm-sarno-lite"
    misplaced = list(sarno.glob("*.md")) + list(sarno.glob("*.sh"))
    misplaced += list((sarno / "data").glob("*.md"))
    for tool in (examples.parent / "tools").iterdir():
        if tool.is_file() and ("sarno" in tool.name.lower() or
                               (tool.suffix in (".py", ".sh") and
                                "wacomm-sarno-lite" in tool.read_text())):
            misplaced.append(tool)
    if misplaced:
        print("Sarno files outside docs/tools: " + ", ".join(map(str, misplaced)))
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
