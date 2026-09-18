#!/usr/bin/env python3

from pathlib import Path
import json
import sys


def main():
    examples = Path(sys.argv[1])
    missing = []

    for configuration in sorted(examples.glob("*/*.json")):
        guide = configuration.with_suffix(".md")
        if not guide.is_file():
            guide = configuration.parent / "docs" / (configuration.stem + ".md")
        if not guide.is_file():
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
            if guide.name != "README.md":
                for section in ("Scientific question", "Prerequisites", "Required forcing fields and units",
                                "Configuration", "Expected outputs", "Verification", "Limitations",
                                "Reproducibility", "References"):
                    if "## " + section not in text:
                        print(f"Missing {section} section in {guide}")
                        return 1
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

    for source_file in sorted(examples.glob("*sources*/*.json")):
        source = json.loads(source_file.read_text())
        if source.get("type") != "FeatureCollection":
            print(f"Source artifact is not a FeatureCollection: {source_file}")
            return 1
        for feature in source.get("features", []):
            properties = feature.get("properties", {})
            if "particlesPerHour" in properties:
                print(f"Legacy particlesPerHour remains in {source_file}")
                return 1
            emission = properties.get("emission", {})
            mode = emission.get("mode")
            required = {"uniform_rate": ("rate", "rate_unit"),
                        "single_pulse": ("particles",),
                        "forcing_interval_batch": ("particles_per_interval",)}
            if mode not in required or any(key not in emission for key in required[mode]):
                print(f"Invalid explicit emission in {source_file}")
                return 1
            if mode == "uniform_rate" and emission["rate_unit"] != "particles/hour":
                print(f"Invalid uniform-rate unit in {source_file}")
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
