import json
import importlib.util
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOKEN_SOURCE = ROOT / "design-system" / "tokens" / "aris.tokens.json"
GENERATOR = ROOT / "tools" / "generate_aris_design_tokens.py"
GENERATED_CSS = ROOT / "src" / "webui" / "src" / "styles" / "harken-search-tokens.generated.css"
GENERATED_CPP = ROOT / "src" / "HarkenSearchDesignTokens.generated.h"
GENERATED_FIGMA = ROOT / "design-system" / "figma" / "aris.variables.csv"
NATIVE_METRICS = ROOT / "src" / "HarkenSearchMetrics.h"
WEB_ENTRY = ROOT / "src" / "webui" / "src" / "main.tsx"


def test_token_source_defines_versioned_brand_and_semantic_contract():
    assert TOKEN_SOURCE.exists(), "Aris needs one machine-readable design-token source"

    payload = json.loads(TOKEN_SOURCE.read_text(encoding="utf-8"))
    assert payload["schemaVersion"] == 1
    assert payload["brand"]["personality"] == ["professional", "restrained", "trustworthy"]

    dark = payload["modes"]["dark-product"]
    assert dark["color.surface.canvas"].startswith("#")
    assert dark["color.text.primary"].startswith("#")
    assert dark["color.action.primary"].startswith("#")
    assert dark["color.signal.measurement"].startswith("#")
    assert dark["color.status.danger"].startswith("#")

    assert payload["metrics"]["control.height.md"] == 28
    assert payload["metrics"]["space.2"] == 8
    assert payload["metrics"]["radius.control"] == 4


def test_generator_renders_web_and_native_tokens_from_the_same_source():
    assert GENERATOR.exists(), "Aris needs a token generator for Web and native UI"

    spec = importlib.util.spec_from_file_location("aris_token_generator", GENERATOR)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)

    payload = json.loads(TOKEN_SOURCE.read_text(encoding="utf-8"))
    css = module.render_css(payload)
    cpp = module.render_cpp(payload)
    figma_csv = module.render_figma_csv(payload)

    assert ':root, [data-aris-theme="dark-product"]' in css
    assert '[data-aris-theme="light-documentation"]' in css
    assert "--aris-color-surface-canvas: #0B0D10;" in css
    assert "--aris-control-height-md: 28px;" in css

    assert "namespace aris::design" in cpp
    assert "kColorSurfaceCanvas" in cpp
    assert "kControlHeightMd = 28.0f" in cpp
    assert figma_csv.startswith("collection,mode,name,type,value,description\n")
    assert "Color,Dark Product,color.surface.canvas,color,#0B0D10," in figma_csv
    assert "Metrics,All,control.height.md,number,28," in figma_csv


def test_committed_generated_resources_are_current():
    result = subprocess.run(
        [sys.executable, str(GENERATOR), "--check"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert GENERATED_CSS.exists()
    assert GENERATED_CPP.exists()
    assert GENERATED_FIGMA.exists()


def test_native_and_web_entry_points_consume_generated_tokens():
    metrics = NATIVE_METRICS.read_text(encoding="utf-8")
    web_entry = WEB_ENTRY.read_text(encoding="utf-8")

    assert '#include "HarkenSearchDesignTokens.generated.h"' in metrics
    assert "kHeightMd = aris::design::kControlHeightMd" in metrics
    assert "kGap8  = aris::design::kSpace2" in metrics
    assert "kRadiusDefault = aris::design::kRadiusControl" in metrics

    generated_import = "import './styles/harken-search-tokens.generated.css';"
    legacy_import = "import './styles/flash-0225.css';"
    assert generated_import in web_entry
    assert web_entry.index(generated_import) < web_entry.index(legacy_import)
