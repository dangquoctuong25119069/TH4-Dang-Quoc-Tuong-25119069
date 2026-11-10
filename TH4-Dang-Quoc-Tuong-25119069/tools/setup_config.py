import argparse
import getpass
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECTS = ["bai1-rest", "bai2-mqtt-pub", "bai3-mqtt-sub-led"]
FLOW = ROOT / "nodered-flow" / "flow.json"
ID_RE = re.compile(r"^[A-Za-z0-9_-]+$")


def set_define(text, name, value):
    pattern = re.compile(rf'^(#define\s+{name}\s+)(".*?"|\S+)', re.M)
    if not pattern.search(text):
        return text
    return pattern.sub(lambda m: m.group(1) + value, text, count=1)


def update_firmware(args):
    for p in PROJECTS:
        cfg = ROOT / p / "include" / "config.h"
        text = cfg.read_text(encoding="utf-8")
        new = text
        if args.sv_id:
            new = set_define(new, "SV_ID", f'"{args.sv_id}"')
        if args.name:
            new = set_define(new, "STUDENT_NAME", f'"{args.name}"')
        if args.mqtt_host:
            new = set_define(new, "MQTT_HOST", f'"{args.mqtt_host}"')
        if args.mqtt_port:
            new = set_define(new, "MQTT_PORT", str(args.mqtt_port))
        if new != text:
            cfg.write_text(new, encoding="utf-8")
            print("cập nhật", cfg.relative_to(ROOT))


def update_flow(args):
    nodes = json.loads(FLOW.read_text(encoding="utf-8"))
    topic_re = re.compile(r"aet2022/[^/\s\"]+/")
    for n in nodes:
        if args.sv_id:
            for key in ("topic", "name", "label"):
                if isinstance(n.get(key), str):
                    n[key] = topic_re.sub(f"aet2022/{args.sv_id}/", n[key])
            if n.get("type") == "ui_tab":
                n["name"] = f"TH4 – {args.sv_id}"
        if n.get("type") == "mqtt-broker":
            if args.mqtt_host:
                n["broker"] = args.mqtt_host
            if args.mqtt_port:
                n["port"] = str(args.mqtt_port)
    FLOW.write_text(json.dumps(nodes, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("cập nhật", FLOW.relative_to(ROOT))


def update_readme(args):
    readme = ROOT / "README.md"
    text = readme.read_text(encoding="utf-8")
    new = text
    if args.name:
        new = re.sub(r"^- Sinh viên: .*$", f"- Sinh viên: {args.name}", new, count=1, flags=re.M)
    if args.sv_id:
        new = re.sub(r"^- Mã SV: .*$", f"- Mã SV: {args.sv_id}", new, count=1, flags=re.M)
    if new != text:
        readme.write_text(new, encoding="utf-8")
        print("cập nhật README.md")


def write_secrets():
    ssid = input("Wi-Fi SSID: ").strip()
    pwd = getpass.getpass("Wi-Fi mật khẩu (không hiện): ")
    for s in (ssid, pwd):
        if '"' in s or "\\" in s:
            raise SystemExit("SSID/mật khẩu có dấu \" hoặc \\ -> sửa tay trong secrets.h")
    body = ("#pragma once\n"
            f'#define WIFI_SSID     "{ssid}"\n'
            f'#define WIFI_PASSWORD "{pwd}"\n')
    for p in PROJECTS:
        f = ROOT / p / "include" / "secrets.h"
        f.write_text(body, encoding="utf-8")
        print("ghi", f.relative_to(ROOT), "(đã nằm trong .gitignore)")


def main():
    ap = argparse.ArgumentParser(description="Cấu hình thông tin sinh viên, Wi-Fi và MQTT.", formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sv-id")
    ap.add_argument("--name")
    ap.add_argument("--mqtt-host")
    ap.add_argument("--mqtt-port", type=int)
    ap.add_argument("--wifi", action="store_true", help="hỏi Wi-Fi và tạo secrets.h")
    args = ap.parse_args()

    if args.sv_id and not ID_RE.match(args.sv_id):
        raise SystemExit("SV_ID chỉ gồm chữ, số, '-' hoặc '_' (không dấu cách, không '/', '#', '+')")
    if args.name and '"' in args.name:
        raise SystemExit('Tên không được chứa dấu "')
    if not any([args.sv_id, args.name, args.mqtt_host, args.mqtt_port, args.wifi]):
        ap.print_help()
        return

    if any([args.sv_id, args.name, args.mqtt_host, args.mqtt_port]):
        update_firmware(args)
    if any([args.sv_id, args.mqtt_host, args.mqtt_port]):
        update_flow(args)
    if args.sv_id or args.name:
        update_readme(args)
    if args.wifi:
        write_secrets()


if __name__ == "__main__":
    main()