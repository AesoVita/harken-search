"""Check measured Info windows from qa_info_bounds.py, including theme spacing."""
import argparse
import json
from pathlib import Path


def violations(report):
    failures = []
    for case in report['cases']:
        windows = case['state']['layout_windows']
        outer = next(w for w in windows if w['name'].split('/')[-1].startswith('RightDockColumn_'))
        content = next(w for w in windows if '/FileInfoContent_' in w['name'] and '/##ArtworkScroll_' not in w['name'])
        artwork = next(w for w in windows if '/##ArtworkScroll_' in w['name'])
        checks = {
            'left inset': content['x'] >= outer['x'] + 8,
            'right inset': content['x'] + content['w'] <= outer['x'] + outer['w'] - 8,
            'title gap': content['y'] >= outer['y'] + 4 + 24 + 8,
            'bottom inset': content['y'] + content['h'] <= outer['y'] + outer['h'] - 4 - 8,
            'artwork fits': artwork['y'] + artwork['h'] <= content['y'] + content['h'],
            'footer stays inside': content['scroll_max_y'] == 0,
            'no outer scroll': outer['scroll_max_y'] == 0,
        }
        failures.extend(f"{case['name']}: {key}" for key, passed in checks.items() if not passed)
    return failures


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    args = parser.parse_args()
    report = json.loads(args.report.read_text(encoding='utf-8'))
    failures = violations(report)
    print(f"{len(report['cases'])} cases; {len(failures)} violations")
    print('\n'.join(failures))
    raise SystemExit(bool(failures))
