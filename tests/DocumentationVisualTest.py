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

# Raster screenshots and figures must be referenced with descriptive alternative text.
import re
images=re.findall(r"!\[([^\]]*)\]\(([^)\s]+)\)",markdown)
for raster in list(figure_directory.rglob("*.png"))+list(figure_directory.rglob("*.jpg")):
    name=raster.relative_to(root).as_posix()
    alts=[alt for alt,target in images if target.endswith(name)]
    assert alts, f"{raster.name} is not embedded by documentation"
    assert all(len(alt.split())>=8 for alt in alts), f"{raster.name} requires descriptive alternative text"

# Every embedded figure in the documentation tree carries descriptive alternative text.
for document in root.glob("*.md"):
    for alt,target in re.findall(r"!\[([^\]]*)\]\(([^)\s]+)\)",document.read_text(encoding="utf-8")):
        assert len(alt.split())>=5, f"{document.name}: {target} requires descriptive alternative text"

# Every documentation page except the reference list itself has a References section.
for document in root.glob("*.md"):
    if document.name!="references.md":
        assert "\n## References" in document.read_text(encoding="utf-8"), f"{document.name} requires a References section"

# The application verification guide identifies its map as synthetic model output.
verification=(root/"application-verification.md").read_text(encoding="utf-8")
assert "$$" in verification and "synthetic" in verification and "not a probability" in verification
