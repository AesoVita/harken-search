from __future__ import annotations

import pytest
import json
from pathlib import Path

from tools import release_update
from tools.build_release_catalog import build_catalog


class PublicRelease:
    def __init__(self, name):
        self.name = name

    def public_get(self, path):
        assert path == "/repos/xindong/harken-search/releases/latest"
        return {
            "id": 3, "tag_name": "v1.0.39", "draft": False, "prerelease": False,
            "assets": [{
                "name": name,
                "browser_download_url": f"https://github.com/xindong/harken-search/releases/download/v1.0.39/{name}",
            } for name in (self.name, self.name + ".sig")],
        }


def test_new_repository_discovery_requires_its_own_manifest_pair():
    assert release_update.discover_stable(
        PublicRelease("manifest-harken-search-win-x64-stable.json"), "xindong/harken-search"
    )["id"] == 3
    with pytest.raises(ValueError, match="stable manifest"):
        release_update.discover_stable(
            PublicRelease("manifest-win-x64-stable.json"), "xindong/harken-search"
        )


def test_release_cli_defaults_to_the_new_repository():
    args = release_update._parser().parse_args(["discover", "--channel", "stable"])
    assert args.repository == "xindong/harken-search"


@pytest.mark.parametrize("repository", ["xindong/harken-search", "xindong/aris"])
def test_catalog_accepts_new_and_historical_repository_receipts(repository):
    fixtures = Path(__file__).parent / "release_contract" / "fixtures"
    source = json.loads((fixtures / "release_catalog_source.json").read_text(encoding="utf-8").replace("xindong/aris", repository))
    catalog = build_catalog(source, source_dir=fixtures)
    assert f"/{repository}/" in catalog["primary_asset"]["url"]


def test_catalog_rejects_cross_repository_asset_and_signature():
    fixtures = Path(__file__).parent / "release_contract" / "fixtures"
    source = json.loads((fixtures / "release_catalog_source.json").read_text(encoding="utf-8"))
    source["signature"]["manifest_url"] = source["signature"]["manifest_url"].replace("xindong/aris", "xindong/harken-search")
    with pytest.raises(ValueError, match="repository"):
        build_catalog(source, source_dir=fixtures)
