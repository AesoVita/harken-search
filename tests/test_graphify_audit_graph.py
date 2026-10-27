import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import graphify_audit_graph  # noqa: E402
import graphify_auto_refresh  # noqa: E402


class GraphifyAuditGraphTests(unittest.TestCase):
    def test_classifies_every_call_site_and_writes_unknown_ledger(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            src = root / "src"
            out = root / "graphify-out" / "main"
            src.mkdir(parents=True)

            (src / "Example.h").write_text(
                """
struct Service {
    virtual void run();
};
void helper();
void caller(Service* service);
""".strip(),
                encoding="utf-8",
            )
            (src / "Example.cpp").write_text(
                """
#include "Example.h"
#include <cstdio>

void helper() {
}

void caller(Service* service) {
    helper();
    service->run();
    std::printf("ok");
    missingGeneratedCall();
}

#if defined(AURO_TEST_POLICY)
void testPolicy() {
    TestSeam::InitWide();
    OtherSeam::StillUnknown();
}
#endif

void sehBoundary() {
    __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}
""".strip(),
                encoding="utf-8",
            )

            graph = graphify_audit_graph.build_audit_graph(src_root=src, repo_root=root)
            graphify_audit_graph.write_outputs(graph, out)

            classifications = {
                call["name"]: call["classification"]
                for call in graph["call_sites"]
                if call["name"] in {
                    "helper", "run", "printf", "missingGeneratedCall",
                    "InitWide", "StillUnknown",
                }
            }
            self.assertEqual(classifications["helper"], "RESOLVED_STATIC")
            self.assertEqual(classifications["run"], "POSSIBLE_DYNAMIC")
            self.assertEqual(classifications["printf"], "EXTERNAL")
            self.assertEqual(classifications["missingGeneratedCall"], "UNKNOWN_BLOCKING")
            self.assertEqual(classifications["InitWide"], "MANUAL_CONTRACT")
            self.assertEqual(classifications["StillUnknown"], "UNKNOWN_BLOCKING")
            self.assertNotIn("defined", {call["name"] for call in graph["call_sites"]})
            self.assertNotIn("__except", {call["name"] for call in graph["call_sites"]})

            self.assertEqual(graph["counts"]["unknown_blocking"], 2)
            self.assertEqual(graph["counts"]["manual_contract"], 1)
            self.assertTrue((out / "audit-call-graph.json").exists())
            ledger = (out / "UNKNOWN_CALLS.md").read_text(encoding="utf-8")
            self.assertIn("missingGeneratedCall", ledger)
            self.assertIn("UNKNOWN_BLOCKING", ledger)

            payload = json.loads((out / "audit-call-graph.json").read_text(encoding="utf-8"))
            self.assertEqual(payload["schema"], "auro-audit-call-graph-v1")

    def test_auto_refresh_main_rebuild_runs_interface_then_audit_graph(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            marker = root / "calls.txt"
            interface_script = root / "interface.py"
            audit_script = root / "audit.py"
            extended_script = root / "extended.py"

            interface_script.write_text(
                f"from pathlib import Path\nPath(r'{marker}').write_text('interface\\n', encoding='utf-8')\n",
                encoding="utf-8",
            )
            audit_script.write_text(
                f"from pathlib import Path\nPath(r'{marker}').write_text(Path(r'{marker}').read_text(encoding='utf-8') + 'audit\\n', encoding='utf-8')\n",
                encoding="utf-8",
            )
            extended_script.write_text(
                f"from pathlib import Path\nPath(r'{marker}').write_text('extended\\n', encoding='utf-8')\n",
                encoding="utf-8",
            )

            old_main = graphify_auto_refresh.MAIN_REFRESH_SCRIPT
            old_audit = graphify_auto_refresh.AUDIT_REFRESH_SCRIPT
            old_extended = graphify_auto_refresh.EXTENDED_REFRESH_SCRIPT
            try:
                graphify_auto_refresh.MAIN_REFRESH_SCRIPT = interface_script
                graphify_auto_refresh.AUDIT_REFRESH_SCRIPT = audit_script
                graphify_auto_refresh.EXTENDED_REFRESH_SCRIPT = extended_script

                self.assertEqual(graphify_auto_refresh.rebuild("main"), 0)
                self.assertEqual(marker.read_text(encoding="utf-8"), "interface\naudit\n")

                self.assertEqual(graphify_auto_refresh.rebuild("extended"), 0)
                self.assertEqual(marker.read_text(encoding="utf-8"), "extended\n")
            finally:
                graphify_auto_refresh.MAIN_REFRESH_SCRIPT = old_main
                graphify_auto_refresh.AUDIT_REFRESH_SCRIPT = old_audit
                graphify_auto_refresh.EXTENDED_REFRESH_SCRIPT = old_extended


if __name__ == "__main__":
    unittest.main()
