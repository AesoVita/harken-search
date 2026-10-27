import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "design-system" / "components" / "aris.components.json"


def test_component_contract_maps_figma_names_to_aris_implementations():
    assert CONTRACT.exists(), "Aris needs a machine-readable component contract"

    payload = json.loads(CONTRACT.read_text(encoding="utf-8"))
    assert payload["schemaVersion"] == 1
    assert payload["tokenSource"] == "../tokens/aris.tokens.json"

    components = {item["id"]: item for item in payload["components"]}
    for component_id in (
        "button",
        "icon-button",
        "text-input",
        "checkbox",
        "radio",
        "switch",
        "select",
        "tooltip",
        "dialog",
        "panel",
    ):
        component = components[component_id]
        assert component["figmaName"].startswith("Aris/")
        assert component["status"] == "implemented"
        assert component["nativeApi"].startswith("aura::ui::")
        assert "default" in component["states"]
        assert "disabled" in component["states"] or component_id in {"tooltip", "dialog", "panel"}

    for component_id in (
        "waveform-preview",
        "timeline",
        "transport-controls",
        "peak-rms-meter",
    ):
        component = components[component_id]
        assert component["status"] == "planned"
        assert component["figmaName"].startswith("Aris/Audio/")

    for component_id in ("scan-job-card", "recovery-banner"):
        component = components[component_id]
        assert component["status"] == "implemented"
        assert component["nativeApi"].startswith("aura::ui::")
        assert component["figmaName"].startswith("Aris/")
