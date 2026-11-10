"""Offline structure checks; does not build firmware or verify hardware."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
errors = []

def require(ok, message):
    if not ok:
        errors.append(message)

configs = []
for project in ('bai1-rest', 'bai2-mqtt-pub', 'bai3-mqtt-sub-led'):
    for name in ('src/main.cpp', 'include/config.h', 'include/secrets.example.h', 'platformio.ini'):
        require((ROOT / project / name).is_file(), f'Missing: {project}/{name}')
    config = (ROOT / project / 'include/config.h').read_text(encoding='utf-8')
    configs.append(dict(re.findall(r'^#define\s+(\w+)\s+"([^"]*)"', config, re.M)))
sv = configs[0]['SV_ID']
require(all(c['SV_ID'] == sv for c in configs), 'Firmware SV_ID mismatch')
nodes = json.loads((ROOT / 'nodered-flow/flow.json').read_text(encoding='utf-8'))
ids = {n['id'] for n in nodes}
require(len(ids) == len(nodes), 'Duplicate flow node IDs')
for n in nodes:
    for key in ('z', 'group', 'tab', 'broker'):
        if key == 'broker' and n['type'] == 'mqtt-broker':
            continue  # hostname, not node reference
        if n.get(key):
            require(n[key] in ids, f'Broken {key}: {n["id"]}')
    for output in n.get('wires', []):
        for target in output:
            require(target in ids, f'Broken wire: {target}')
    if n.get('topic', '').startswith('aet2022/'):
        require(n['topic'].startswith(f'aet2022/{sv}/'), f'Topic mismatch: {n["topic"]}')
    if n['type'] == 'mqtt-broker':
        require(all(c['MQTT_HOST'] == n['broker'] for c in configs[1:]), 'Broker mismatch')
for p in ROOT.rglob('*.md'):
    for target in re.findall(r'\]\(([^)]+)\)', p.read_text(encoding='utf-8')):
        if '://' in target or target.startswith('#'):
            continue
        require((p.parent / target.split('#')[0]).exists(), f'Broken link: {p.relative_to(ROOT)} -> {target}')
if errors:
    print('\n'.join(errors))
    raise SystemExit(1)
print(f'PASS: 3 projects, {len(nodes)} flow nodes, IDs/topics/broker, Markdown links.')
print('Hardware evidence and firmware compilation still require local validation.')
