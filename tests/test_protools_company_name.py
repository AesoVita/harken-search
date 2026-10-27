from pathlib import Path
source = Path("src/HarkenSearchProToolsBridgeMain.cpp").read_text(encoding="utf-8")
expected = '{"company_name":"Harken Search","application_name":"HarkenSearchProToolsBridge"}'
if expected not in source or '"company_name":"Aris"' in source:
    raise SystemExit("FAIL Pro Tools company name")
print("PASS Pro Tools company name")
