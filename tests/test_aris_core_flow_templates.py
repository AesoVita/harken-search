import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FLOWS = ROOT / "design-system" / "patterns" / "aris.core-flows.json"
COMPONENTS = ROOT / "design-system" / "components" / "aris.components.json"
FIGMA = ROOT / "design-system" / "figma" / "aris.figma-manifest.json"


def test_core_flow_manifest_defines_five_real_desktop_templates():
    assert FLOWS.exists(), "Aris needs a machine-readable core-flow template manifest"
    payload = json.loads(FLOWS.read_text(encoding="utf-8"))

    assert payload["schemaVersion"] == 1
    assert payload["version"] == "0.1.0"
    flows = payload["flows"]
    assert len(flows) == 5
    assert {flow["id"] for flow in flows} == {
        "browse-search",
        "scan-process",
        "audition-waveform",
        "batch-recovery",
        "daw-handoff",
    }

    for flow in flows:
        assert flow["figmaTemplate"].startswith("Aris/Flows/")
        assert flow["status"] in {"implemented", "partial", "planned"}
        assert flow["userOutcome"]
        assert flow["componentIds"]
        assert flow["nativeEvidence"]
        assert set(flow["qaStates"]) >= {"default", "loading", "empty", "error"}
        assert flow["desktopViewport"] == {"width": 1440, "height": 900}


def test_flow_components_and_figma_handoff_share_the_same_contract_ids():
    flow_payload = json.loads(FLOWS.read_text(encoding="utf-8"))
    component_payload = json.loads(COMPONENTS.read_text(encoding="utf-8"))
    figma_payload = json.loads(FIGMA.read_text(encoding="utf-8"))

    component_ids = {component["id"] for component in component_payload["components"]}
    for flow in flow_payload["flows"]:
        assert set(flow["componentIds"]) <= component_ids

    assert figma_payload["patternSource"] == "../patterns/aris.core-flows.json"
    assert "../patterns/aris.core-flows.json" in figma_payload["generatedArtifacts"]
