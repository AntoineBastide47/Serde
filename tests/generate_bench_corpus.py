#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


DEFAULT_MIN_BYTES = 5 * 1024 * 1024

WORDS = (
    "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu "
    "network browser render cache storage worker request response profile "
    "compile package module dependency trace event pipeline metadata"
).split()

UNICODE_TEXT = [
    "Bonjour, deja vu, facade, eleve",
    "Gruezi mitenand, Zuerich, Chur, Geneve",
    "こんにちは世界",
    "Привет мир",
    "مرحبا بالعالم",
    "नमस्ते दुनिया",
    "שלום עולם",
    "😀 data 🚀 parser ✅",
]


def dump(value, ensure_ascii=False):
    return json.dumps(value, ensure_ascii=ensure_ascii, separators=(",", ":"))


def sentence(i, words=24):
    return " ".join(WORDS[(i + j) % len(WORDS)] for j in range(words))


def write_array(path, min_bytes, make_row, ensure_ascii=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as out:
        out.write("[")
        i = 0
        first = True
        while out.tell() < min_bytes - 1:
            if not first:
                out.write(",")
            out.write(dump(make_row(i), ensure_ascii=ensure_ascii))
            first = False
            i += 1
        out.write("]")
    return path.stat().st_size


def write_object(path, min_bytes, make_pair, ensure_ascii=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as out:
        out.write("{")
        i = 0
        first = True
        while out.tell() < min_bytes - 1:
            key, value = make_pair(i)
            if not first:
                out.write(",")
            out.write(dump(key))
            out.write(":")
            out.write(dump(value, ensure_ascii=ensure_ascii))
            first = False
            i += 1
        out.write("}")
    return path.stat().st_size


def large_array_objects(path, min_bytes):
    def row(i):
        return {
            "id": f"obj-{i:08d}",
            "kind": ["user", "device", "document", "task"][i % 4],
            "active": i % 7 != 0,
            "score": round((i * 17 % 10000) / 37.0, 4),
            "tags": [f"tag-{(i + j) % 97}" for j in range(5)],
            "owner": {
                "team": f"team-{i % 31}",
                "region": ["us", "eu", "apac", "latam"][i % 4],
            },
            "description": sentence(i, 34),
        }

    return write_array(path, min_bytes, row)


def large_object_many_keys(path, min_bytes):
    def pair(i):
        return f"feature.flag.{i:08d}", {
            "enabled": i % 3 == 0,
            "rollout": i % 100,
            "owner": f"owner-{i % 211}",
            "notes": sentence(i, 18),
        }

    return write_object(path, min_bytes, pair)


def escaped_strings(path, min_bytes):
    fragments = [
        'quoted "value" inside text',
        "path C:\\Program Files\\Serde\\bench\\input.json",
        "line one\nline two\nline three",
        "tab\tseparated\tfields",
        "unicode snowman ☃ and music 𝄞",
    ]

    def row(i):
        text = " | ".join(fragments[(i + j) % len(fragments)] for j in range(8))
        return {
            "id": i,
            "escaped": text,
            "regex": r"^([A-Za-z0-9_]+)\\s*=\\s*\"([^\"]*)\"$",
            "json_fragment": "{\"name\":\"value\",\"enabled\":true}",
        }

    return write_array(path, min_bytes, row, ensure_ascii=True)


def unicode_strings(path, min_bytes):
    def row(i):
        return {
            "id": i,
            "locale": ["fr-CH", "ja-JP", "ru-RU", "ar-SA", "hi-IN", "he-IL"][i % 6],
            "title": UNICODE_TEXT[i % len(UNICODE_TEXT)],
            "body": " / ".join(UNICODE_TEXT[(i + j) % len(UNICODE_TEXT)] for j in range(14)),
        }

    return write_array(path, min_bytes, row, ensure_ascii=False)


def numbers_heavy(path, min_bytes):
    def row(i):
        return [
            i,
            -i,
            i % 997,
            round((i * 3.1415926535) / 17.0, 9),
            1.0e-9 * (i + 1),
            6.022e23 / (i + 1),
        ]

    return write_array(path, min_bytes, row)


def nested(path, min_bytes):
    depth = 256
    payload = "x" * max(1024, min_bytes // depth)
    node = {"leaf": True, "payload": payload}
    for i in range(depth):
        node = {
            "level": depth - i,
            "name": f"node-{depth - i}",
            "payload": payload,
            "child": node,
        }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(dump(node), encoding="utf-8")
    return path.stat().st_size


def config_lockfile(path, min_bytes):
    prefix = '{"name":"serde-bench-fixture","lockfileVersion":3,"requires":true,"packages":{'
    suffix = '},"dependencies":{}}'

    def package(i):
        return f"node_modules/pkg-{i:06d}", {
            "version": f"{i % 19}.{i % 31}.{i % 43}",
            "resolved": f"https://registry.example.invalid/pkg-{i:06d}.tgz",
            "integrity": f"sha512-{sentence(i, 12).replace(' ', '')}",
            "license": ["MIT", "BSD-3-Clause", "Apache-2.0", "ISC"][i % 4],
            "dependencies": {f"dep-{(i + j) % 1000:04d}": f"^{j}.{i % 10}.0" for j in range(5)},
        }

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as out:
        out.write(prefix)
        i = 0
        first = True
        while out.tell() + len(suffix) < min_bytes:
            key, value = package(i)
            if not first:
                out.write(",")
            out.write(dump(key))
            out.write(":")
            out.write(dump(value))
            first = False
            i += 1
        out.write(suffix)
    return path.stat().st_size


def trace_events(path, min_bytes):
    prefix = '{"traceEvents":['
    suffix = '],"metadata":{"benchmark":"serde trace fixture","clock-domain":"MONOTONIC"}}'

    def event(i):
        return {
            "name": ["Parse", "Layout", "Paint", "Commit", "GC", "Compile"][i % 6],
            "cat": ["benchmark", "renderer", "v8", "network"][i % 4],
            "ph": ["B", "E", "X", "i"][i % 4],
            "ts": i * 17.125,
            "dur": (i % 1000) / 13.0,
            "pid": i % 32,
            "tid": i % 512,
            "args": {
                "url": f"https://example.invalid/page/{i % 10000}",
                "bytes": i * 97 % 1000000,
                "cached": i % 5 == 0,
            },
        }

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as out:
        out.write(prefix)
        i = 0
        first = True
        while out.tell() + len(suffix) < min_bytes:
            if not first:
                out.write(",")
            out.write(dump(event(i)))
            first = False
            i += 1
        out.write(suffix)
    return path.stat().st_size


def duplicate_keys(path, min_bytes):
    def pair(i):
        key = "duplicate" if i % 3 else f"unique-{i:08d}"
        return key, {
            "index": i,
            "text": sentence(i, 16),
            "enabled": i % 2 == 0,
        }

    return write_object(path, min_bytes, pair)


GENERATORS = {
    "large-array-objects.json": large_array_objects,
    "large-object-many-keys.json": large_object_many_keys,
    "escaped-strings.json": escaped_strings,
    "unicode-strings.json": unicode_strings,
    "numbers-heavy.json": numbers_heavy,
    "nested.json": nested,
    "config-lockfile.json": config_lockfile,
    "trace-events.json": trace_events,
    "duplicate-keys.json": duplicate_keys,
}


def main():
    parser = argparse.ArgumentParser(description="Generate large JSON benchmark fixtures.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "bench",
        help="Directory for generated benchmark JSON files.",
    )
    parser.add_argument(
        "--min-mb",
        type=float,
        default=5.0,
        help="Minimum size for each generated file.",
    )
    args = parser.parse_args()

    min_bytes = int(args.min_mb * 1024 * 1024)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    for filename, generate in GENERATORS.items():
        path = args.output_dir / filename
        size = generate(path, min_bytes)
        if size < min_bytes:
            raise SystemExit(f"{filename} is too small: {size} bytes")
        print(f"{filename}: {size / (1024 * 1024):.2f} MiB")


if __name__ == "__main__":
    main()
