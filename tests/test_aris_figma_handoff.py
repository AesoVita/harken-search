import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "design-system" / "figma" / "aris.figma-manifest.json"
VARIABLES = ROOT / "design-system" / "figma" / "aris.variables.csv"


def test_figma_handoff_manifest_defines_the_library_and_variable_modes():
    assert MANIFEST.exists(), "Aris needs a Figma handoff manifest"
    payload = json.loads(MANIFEST.read_text(encoding="utf-8"))

    assert payload["schemaVersion"] == 1
    assert payload["project"] == "Aris Design System"
    assert payload["libraryFile"] == "Aris Component Library"
    assert payload["variableCollections"]["Color"]["modes"] == [
        "Dark Product",
        "Light Documentation",
    ]
    assert "00 Governance" in payload["pages"]
    assert "03 Components" in payload["pages"]
    assert payload["externalPublishRequiresHumanApproval"] is True


def test_figma_variables_export_is_present_and_uses_semantic_names():
    assert VARIABLES.exists(), "Aris needs a Figma Variables import artifact"
    rows = VARIABLES.read_text(encoding="utf-8").splitlines()
    assert rows[0] == "collection,mode,name,type,value,description"
    assert any(
        row.startswith("Color,Dark Product,color.surface.canvas,color,#0B0D10,")
        for row in rows[1:]
    )
    assert any(
        row.startswith("Metrics,All,control.height.md,number,28,")
        for row in rows[1:]
    )

