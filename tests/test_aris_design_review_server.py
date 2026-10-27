import json
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ArisDesignReviewServerTests(unittest.TestCase):
    def test_review_model_uses_the_versioned_design_system_sources(self):
        from tools.aris_design_review_server import build_review_model

        model = build_review_model(ROOT)

        self.assertEqual(model["brand"]["name"], "Aris Design System")
        self.assertEqual(len(model["components"]), 16)
        self.assertEqual(
            {component["status"] for component in model["components"]},
            {"implemented", "planned"},
        )
        self.assertEqual(len(model["flows"]), 5)
        self.assertEqual({flow["status"] for flow in model["flows"]}, {"partial"})

    def test_review_html_exposes_truthful_statuses_and_all_five_flows(self):
        from tools.aris_design_review_server import build_review_model, render_review_html

        html = render_review_html(build_review_model(ROOT))

        self.assertIn('data-review-status="read-only"', html)
        for status in ("implemented", "partial", "planned"):
            self.assertIn(f'data-status="{status}"', html)
        for flow_id in (
            "browse-search",
            "scan-process",
            "audition-waveform",
            "batch-recovery",
            "daw-handoff",
        ):
            self.assertIn(f'id="flow-{flow_id}"', html)
        self.assertIn("Human review required", html)

    def test_http_surface_is_read_only(self):
        from tools.aris_design_review_server import create_server

        server = create_server(ROOT, host="127.0.0.1", port=0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            url = f"http://127.0.0.1:{server.server_port}/"
            with urllib.request.urlopen(url, timeout=3) as response:
                self.assertEqual(response.status, 200)
                self.assertIn("Aris Design Review", response.read().decode("utf-8"))
            request = urllib.request.Request(url, data=b"write", method="POST")
            with self.assertRaises(urllib.error.HTTPError) as error:
                urllib.request.urlopen(request, timeout=3)
            self.assertEqual(error.exception.code, 405)
            error.exception.close()
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=3)

    def test_ctest_registration_is_present_once(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertEqual(cmake.count("NAME TestArisDesignReviewServer"), 1)

    def test_powershell_launcher_starts_hidden_and_prints_the_review_url(self):
        launcher = ROOT / "tools/start_aris_design_review.ps1"
        self.assertTrue(launcher.exists())
        source = launcher.read_text(encoding="utf-8")
        self.assertIn("-WindowStyle Hidden", source)
        self.assertIn('Get-Command "python" -CommandType Application | Select-Object -First 1', source)
        self.assertIn("-TimeoutSec 1", source)
        self.assertIn("http://127.0.0.1:$Port/health", source)
        self.assertIn("http://127.0.0.1:$Port/", source)


if __name__ == "__main__":
    unittest.main()
