"""Serve a local, read-only review of the versioned Aris design-system assets."""

from __future__ import annotations

import argparse
import csv
import html
import json
from collections import Counter, defaultdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit


def _load_json(path: Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise RuntimeError(f"Expected a JSON object: {path}")
    return payload


def build_review_model(root: Path) -> dict[str, Any]:
    tokens = _load_json(root / "design-system/tokens/aris.tokens.json")
    components = _load_json(root / "design-system/components/aris.components.json")
    flows = _load_json(root / "design-system/patterns/aris.core-flows.json")
    figma = _load_json(root / "design-system/figma/aris.figma-manifest.json")
    with (root / "design-system/figma/aris.variables.csv").open(encoding="utf-8", newline="") as source:
        variable_rows = list(csv.DictReader(source))

    component_items = list(components.get("components", []))
    flow_items = list(flows.get("flows", []))
    return {
        "brand": {
            "name": figma["project"],
            "library": figma["libraryFile"],
            "personality": tokens["brand"]["personality"],
            "principles": tokens["brand"]["principles"],
            "version": components["version"],
        },
        "modes": tokens["modes"],
        "metrics": tokens["metrics"],
        "typography": tokens["typography"],
        "motion": tokens["motion"],
        "variableCollections": figma["variableCollections"],
        "variableCount": len(variable_rows),
        "components": component_items,
        "componentCounts": dict(Counter(item["status"] for item in component_items)),
        "flows": flow_items,
        "flowCounts": dict(Counter(item["status"] for item in flow_items)),
        "humanApprovalGates": figma["humanApprovalGates"],
    }


def _e(value: object) -> str:
    return html.escape(str(value), quote=True)


def _status_badge(status: str) -> str:
    return f'<span class="status" data-status="{_e(status)}"><i></i>{_e(status)}</span>'


def _token_rows(mode_name: str, values: dict[str, str]) -> str:
    rows = []
    for name, value in values.items():
        rows.append(
            '<div class="token-row">'
            f'<span class="swatch" style="--swatch:{_e(value)}"></span>'
            f'<span class="token-name">{_e(name.removeprefix("color."))}</span>'
            f'<code>{_e(value)}</code>'
            '</div>'
        )
    return f'<section class="token-mode"><h3>{_e(mode_name.replace("-", " "))}</h3>{"".join(rows)}</section>'


def _component_groups(components: list[dict[str, Any]]) -> str:
    groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for component in components:
        groups[str(component["category"])].append(component)
    sections = []
    for category, items in groups.items():
        entries = []
        for item in items:
            api = item.get("nativeApi") or "No native API yet"
            states = " · ".join(item.get("states", []))
            entries.append(
                f'<article class="component-row" data-status="{_e(item["status"])}">'
                '<div class="component-title">'
                f'<strong>{_e(item["figmaName"].split("/")[-1])}</strong>'
                f'<span>{_e(item["figmaName"])}</span>'
                '</div>'
                f'{_status_badge(item["status"])}'
                f'<code>{_e(api)}</code>'
                f'<p>{_e(states)}</p>'
                '</article>'
            )
        sections.append(
            f'<section class="component-group"><h3>{_e(category)}</h3><div>{"".join(entries)}</div></section>'
        )
    return "".join(sections)


def _flow_cards(flows: list[dict[str, Any]]) -> str:
    cards = []
    for index, flow in enumerate(flows, start=1):
        regions = "".join(f'<span>{_e(region)}</span>' for region in flow["regions"])
        states = "".join(f'<li>{_e(state)}</li>' for state in flow["qaStates"])
        evidence = "".join(f'<li><code>{_e(item)}</code></li>' for item in flow["nativeEvidence"])
        cards.append(
            f'<article class="flow" id="flow-{_e(flow["id"])}" data-status="{_e(flow["status"])}">'
            '<header>'
            f'<span class="flow-index">0{index}</span><div><p>{_e(flow["figmaTemplate"])}</p>'
            f'<h3>{_e(flow["id"].replace("-", " "))}</h3></div>{_status_badge(flow["status"])}'
            '</header>'
            f'<p class="outcome">{_e(flow["userOutcome"])}</p>'
            f'<div class="flow-map">{regions}</div>'
            '<div class="flow-details">'
            f'<div><h4>QA states</h4><ul class="state-list">{states}</ul></div>'
            f'<div><h4>Native evidence</h4><ul class="evidence-list">{evidence}</ul></div>'
            '</div></article>'
        )
    return "".join(cards)


def render_review_html(model: dict[str, Any]) -> str:
    dark_tokens = _token_rows("dark-product", model["modes"]["dark-product"])
    light_tokens = _token_rows("light-documentation", model["modes"]["light-documentation"])
    components = _component_groups(model["components"])
    flows = _flow_cards(model["flows"])
    principles = "".join(f'<li>{_e(item)}</li>' for item in model["brand"]["principles"])
    gates = "".join(f'<li>{_e(item)}</li>' for item in model["humanApprovalGates"])
    implemented = model["componentCounts"].get("implemented", 0)
    planned = model["componentCounts"].get("planned", 0)
    partial = model["flowCounts"].get("partial", 0)
    template = r'''<!doctype html>
<html lang="zh-CN" data-aris-theme="dark-product">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Aris Design Review</title>
<style>
:root{--canvas:#0B0D10;--panel:#11151A;--elevated:#181D24;--input:#0F1318;--hover:#1C232B;--text:#F2F5F7;--secondary:#A7B0BA;--muted:#6F7984;--border:#252B33;--strong:#37414C;--action:#E59A45;--actionText:#17120D;--focus:#80B7F0;--signal:#55B7C4;--success:#65B87A;--warning:#D6A54A;--danger:#D96B6B;--info:#6C9ED8;--font:"Segoe UI Variable","Segoe UI","Microsoft YaHei UI",sans-serif;--mono:"Cascadia Mono",Consolas,monospace}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;background:var(--canvas);color:var(--text);font:13px/1.55 var(--font);letter-spacing:.006em}button,a{font:inherit}.skip{position:fixed;left:12px;top:-60px;z-index:10;padding:10px 14px;background:var(--focus);color:#0b1219}.skip:focus{top:12px}.shell{display:grid;grid-template-columns:224px minmax(0,1fr);min-height:100vh}.rail{position:sticky;top:0;height:100vh;padding:24px 18px;border-right:1px solid var(--border);background:var(--panel)}.brand-mark{display:flex;align-items:center;gap:10px;margin-bottom:34px}.brand-mark b{display:grid;place-items:center;width:30px;height:30px;border-radius:6px;background:var(--action);color:var(--actionText);font-size:15px}.brand-mark span{font-weight:600}.brand-mark small{display:block;color:var(--muted);font-size:10px;font-weight:400}.rail nav{display:grid;gap:3px}.rail nav a{padding:8px 10px;border-radius:4px;color:var(--secondary);text-decoration:none}.rail nav a:hover,.rail nav a:focus-visible{background:var(--hover);color:var(--text);outline:1px solid var(--strong)}.read-only{position:absolute;left:18px;right:18px;bottom:22px;padding-top:14px;border-top:1px solid var(--border);color:var(--muted);font-size:11px}.read-only strong{display:block;color:var(--success);font-weight:600}.content{min-width:0;padding:0 44px 80px}.masthead{display:grid;grid-template-columns:minmax(0,1.5fr) minmax(280px,.6fr);gap:36px;padding:66px 0 46px;border-bottom:1px solid var(--border)}.eyebrow{margin:0 0 12px;color:var(--action);font-size:11px;font-weight:600;letter-spacing:.16em;text-transform:uppercase}.masthead h1{max-width:850px;margin:0;font-size:clamp(38px,6vw,74px);line-height:.98;letter-spacing:-.045em;font-weight:600}.masthead .intro{max-width:62ch;margin:24px 0 0;color:var(--secondary);font-size:15px}.approval{align-self:end;padding:18px 0 0 20px;border-left:2px solid var(--warning)}.approval strong{display:block;color:var(--warning);font-size:12px}.approval p{margin:6px 0 0;color:var(--secondary)}.section{padding:48px 0;border-bottom:1px solid var(--border)}.section-head{display:flex;align-items:end;justify-content:space-between;gap:24px;margin-bottom:24px}.section-head h2{margin:0;font-size:24px;font-weight:600;letter-spacing:-.02em}.section-head p{max-width:56ch;margin:0;color:var(--muted)}.truth-grid{display:grid;grid-template-columns:1.1fr repeat(3,minmax(130px,.45fr));border-top:1px solid var(--strong);border-bottom:1px solid var(--strong)}.truth-grid>div{padding:22px 20px;border-right:1px solid var(--border)}.truth-grid>div:last-child{border:0}.truth-grid .statement{padding-left:0}.truth-grid strong{display:block;font-size:30px;font-variant-numeric:tabular-nums}.truth-grid span{color:var(--muted)}.truth-grid p{margin:0;color:var(--secondary);font-size:14px}.status{display:inline-flex;align-items:center;gap:7px;color:var(--secondary);font-size:10px;font-weight:600;letter-spacing:.08em;text-transform:uppercase}.status i{width:7px;height:7px;border-radius:50%;background:var(--muted)}.status[data-status="implemented"] i{background:var(--success)}.status[data-status="partial"] i{background:var(--warning)}.status[data-status="planned"] i{background:var(--info)}.principles{display:flex;flex-wrap:wrap;gap:8px;padding:0;list-style:none}.principles li{padding:6px 10px;border:1px solid var(--strong);border-radius:999px;color:var(--secondary)}.token-board{display:grid;grid-template-columns:1fr 1fr;gap:28px}.token-mode h3,.component-group h3{margin:0 0 12px;color:var(--secondary);font-size:11px;letter-spacing:.12em;text-transform:uppercase}.token-row{display:grid;grid-template-columns:18px minmax(0,1fr) auto;align-items:center;gap:10px;min-height:32px;border-bottom:1px solid var(--border)}.swatch{width:14px;height:14px;border-radius:3px;background:var(--swatch);box-shadow:0 0 0 1px var(--strong)}.token-name{color:var(--secondary)}code{font:11px/1.45 var(--mono);color:var(--muted)}.component-group{margin-top:30px}.component-group>div{border-top:1px solid var(--strong)}.component-row{display:grid;grid-template-columns:minmax(170px,1fr) 100px minmax(180px,.8fr) minmax(220px,1.3fr);align-items:center;gap:18px;min-height:62px;border-bottom:1px solid var(--border)}.component-title strong,.component-title span{display:block}.component-title strong{font-size:14px}.component-title span{color:var(--muted);font-size:11px}.component-row p{margin:0;color:var(--muted);font-size:11px}.flows{display:grid;gap:16px}.flow{padding:24px;border:1px solid var(--strong);border-radius:6px;background:var(--panel)}.flow header{display:grid;grid-template-columns:44px minmax(0,1fr) auto;align-items:center;gap:14px}.flow-index{color:var(--action);font:12px var(--mono)}.flow header p{margin:0;color:var(--muted);font-size:10px}.flow h3{margin:2px 0 0;font-size:20px;text-transform:capitalize}.outcome{max-width:72ch;margin:18px 0;color:var(--secondary);font-size:14px}.flow-map{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:2px;padding:2px;background:var(--border)}.flow-map span{display:grid;place-items:center;min-height:70px;padding:10px;background:var(--elevated);color:var(--secondary);text-align:center}.flow-details{display:grid;grid-template-columns:.6fr 1.4fr;gap:28px;margin-top:20px}.flow-details h4{margin:0 0 8px;color:var(--muted);font-size:10px;letter-spacing:.1em;text-transform:uppercase}.state-list{display:flex;flex-wrap:wrap;gap:5px;margin:0;padding:0;list-style:none}.state-list li{padding:4px 7px;background:var(--elevated);color:var(--secondary);font-size:10px}.evidence-list{display:grid;gap:4px;margin:0;padding:0;list-style:none}.gate-list{display:grid;grid-template-columns:repeat(2,1fr);gap:0;margin:0;padding:0;border-top:1px solid var(--strong);list-style:none}.gate-list li{padding:16px 0;border-bottom:1px solid var(--border);color:var(--secondary)}.gate-list li:nth-child(odd){padding-right:24px}.gate-list li:nth-child(even){padding-left:24px;border-left:1px solid var(--border)}footer{padding:30px 0;color:var(--muted)}
@media(max-width:980px){.shell{grid-template-columns:1fr}.rail{position:relative;height:auto;border-right:0;border-bottom:1px solid var(--border)}.rail nav{display:flex;flex-wrap:wrap}.read-only{position:static;margin-top:16px}.content{padding-inline:22px}.masthead{grid-template-columns:1fr}.truth-grid{grid-template-columns:1fr 1fr}.truth-grid .statement{grid-column:1/-1}.component-row{grid-template-columns:1fr auto}.component-row code,.component-row p{grid-column:1/-1}.token-board{grid-template-columns:1fr}}
@media(max-width:620px){.content{padding-inline:16px}.masthead{padding-top:38px}.truth-grid{grid-template-columns:1fr}.truth-grid .statement{grid-column:auto}.truth-grid>div{border-right:0;border-bottom:1px solid var(--border);padding-left:0}.flow{padding:18px}.flow-map{grid-template-columns:1fr 1fr}.flow-details,.gate-list{grid-template-columns:1fr}.gate-list li:nth-child(even){padding-left:0;border-left:0}.token-row{grid-template-columns:18px 1fr}.token-row code{grid-column:2}.flow header{grid-template-columns:34px minmax(0,1fr)}.flow header .status{grid-column:2}}
@media(prefers-reduced-motion:reduce){html{scroll-behavior:auto}}
</style>
</head>
<body data-review-status="read-only">
<a class="skip" href="#review">跳到审阅内容</a>
<div class="shell">
<aside class="rail"><div class="brand-mark"><b>A</b><span>Aris<small>design evidence</small></span></div><nav><a href="#truth">Status truth</a><a href="#brand">Brand</a><a href="#tokens">Variables</a><a href="#components">Components</a><a href="#flows">Five flows</a><a href="#gates">Approval gates</a></nav><div class="read-only"><strong>● READ ONLY</strong>Rendered from versioned repository sources.</div></aside>
<main class="content" id="review">
<header class="masthead"><div><p class="eyebrow">Design system review · v__VERSION__</p><h1>Studio precision,<br>without pretending.</h1><p class="intro">A truthful review surface for the Aris workstation. Implemented controls, partial workflows, and planned audio primitives remain visibly distinct.</p></div><aside class="approval"><strong>Human review required</strong><p>This page is evidence, not a Figma library and not a product-completion claim.</p></aside></header>
<section class="section" id="truth"><div class="section-head"><div><p class="eyebrow">Status truth</p><h2>What exists today</h2></div><p>Counts come directly from the component and flow contracts.</p></div><div class="truth-grid"><div class="statement"><p>__BRAND__ is a mature commercial workstation: calm, precise, dense, readable.</p></div><div data-status="implemented"><strong>__IMPLEMENTED__</strong><span>implemented components</span></div><div data-status="partial"><strong>__PARTIAL__</strong><span>partial desktop flows</span></div><div data-status="planned"><strong>__PLANNED__</strong><span>planned audio controls</span></div></div></section>
<section class="section" id="brand"><div class="section-head"><div><p class="eyebrow">Brand foundation</p><h2>Signal first. Quiet intelligence.</h2></div><p>Functional color carries playback, measurement, focus, and status. Decoration stays subordinate.</p></div><ul class="principles">__PRINCIPLES__</ul></section>
<section class="section" id="tokens"><div class="section-head"><div><p class="eyebrow">Variables · __VARIABLE_COUNT__ records</p><h2>Two explicit modes</h2></div><p>Dark Product is the workstation surface. Light Documentation exists for evidence and handoff.</p></div><div class="token-board">__TOKENS__</div></section>
<section class="section" id="components"><div class="section-head"><div><p class="eyebrow">Component ledger</p><h2>Native mappings and gaps</h2></div><p>Planned entries intentionally show “No native API yet.”</p></div>__COMPONENTS__</section>
<section class="section" id="flows"><div class="section-head"><div><p class="eyebrow">Product screens</p><h2>Five desktop workflows</h2></div><p>Every flow remains partial until its complete visual and behavioral acceptance is reviewed.</p></div><div class="flows">__FLOWS__</div></section>
<section class="section" id="gates"><div class="section-head"><div><p class="eyebrow">Governance</p><h2>Approval stays human</h2></div><p>Repository evidence can prepare a decision; it cannot make the decision.</p></div><ul class="gate-list">__GATES__</ul></section>
<footer>Aris Design Review · local read-only evidence · generated from repository contracts</footer>
</main></div></body></html>'''
    return (template.replace("__VERSION__", _e(model["brand"]["version"]))
            .replace("__BRAND__", _e(model["brand"]["name"]))
            .replace("__IMPLEMENTED__", str(implemented))
            .replace("__PARTIAL__", str(partial))
            .replace("__PLANNED__", str(planned))
            .replace("__PRINCIPLES__", principles)
            .replace("__VARIABLE_COUNT__", str(model["variableCount"]))
            .replace("__TOKENS__", dark_tokens + light_tokens)
            .replace("__COMPONENTS__", components)
            .replace("__FLOWS__", flows)
            .replace("__GATES__", gates))


def create_server(root: Path, *, host: str, port: int) -> ThreadingHTTPServer:
    model = build_review_model(root)
    document = render_review_html(model).encode("utf-8")

    class ReadOnlyReviewHandler(BaseHTTPRequestHandler):
        def _headers(self, status: int, content_type: str, length: int) -> None:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(length))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'")
            self.end_headers()

        def do_GET(self) -> None:  # noqa: N802
            path = urlsplit(self.path).path
            if path == "/health":
                payload = json.dumps({"ok": True, "service": "aris-design-review", "readOnly": True}).encode("utf-8")
                self._headers(200, "application/json; charset=utf-8", len(payload))
                self.wfile.write(payload)
                return
            if path not in ("/", "/index.html"):
                self.send_error(404)
                return
            self._headers(200, "text/html; charset=utf-8", len(document))
            self.wfile.write(document)

        def do_HEAD(self) -> None:  # noqa: N802
            self._headers(200, "text/html; charset=utf-8", len(document))

        def _reject_write(self) -> None:
            self.send_error(405, "Aris design review is read-only")

        do_POST = _reject_write
        do_PUT = _reject_write
        do_PATCH = _reject_write
        do_DELETE = _reject_write

        def log_message(self, format: str, *args: object) -> None:
            return

    return ThreadingHTTPServer((host, port), ReadOnlyReviewHandler)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Serve the local read-only Aris design review.")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4320)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    server = create_server(Path(args.root).resolve(), host=args.host, port=args.port)
    print(f"Aris design review ready: http://{args.host}:{server.server_port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
