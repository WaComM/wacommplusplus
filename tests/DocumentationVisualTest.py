import pathlib
import sys
import xml.etree.ElementTree as ET


root=pathlib.Path(sys.argv[1])
figure_directory=root/"figures"
documents=list(root.glob("*.md"))+list((root.parent/"examples").rglob("*.md"))
markdown="\n".join(document.read_text(encoding="utf-8") for document in documents)
figures=list(figure_directory.rglob("*.svg"))
assert figures, "documentation must contain versioned SVG figures"
namespace={"svg":"http://www.w3.org/2000/svg"}
for figure in figures:
    tree=ET.parse(figure)
    svg=tree.getroot()
    assert svg.tag=="{http://www.w3.org/2000/svg}svg"
    assert svg.find("svg:title",namespace) is not None, f"{figure.name} requires an accessible title"
    assert svg.find("svg:desc",namespace) is not None, f"{figure.name} requires an accessible description"
    assert figure.relative_to(root).as_posix() in markdown, f"{figure.name} is not referenced by documentation"
assert "$$" in (root/"opendrift-comparison.md").read_text(encoding="utf-8"), "comparison requires a displayed equation"
assert "not georeferenced" in (root/"figures/coupled-drift-map.svg").read_text(encoding="utf-8").lower()
