#!/usr/bin/env python3
# Copyright 2024-2026 keplertech.io
# SPDX-License-Identifier: GPL-3.0-only

"""Check configured wheel IDs against vendored NajaEDA, without building wheels.

CI-only dependencies: cibuildwheel (the workflow's version) and PyYAML.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
import sys

def check_release_jobs(workflow: dict) -> set[str]:
    """Keep release-only provider tests and published artifacts coupled."""
    build = workflow["jobs"]["build"]
    matrix = build["strategy"]["matrix"]
    # Deliberately accept this limited gate, not arbitrary Actions expressions.
    gate = re.fullmatch(
        r"\$\{\{\s*fromJSON\(\s*github\.event_name\s*==\s*'workflow_dispatch'"
        r"\s*&&\s*inputs\.publish\s*&&\s*'([^']+)'\s*\|\|\s*'([^']+)'\s*\)\s*\}\}",
        matrix.get("provider", ""),
    )
    if not gate or [json.loads(value) for value in gate.groups()] != [
        ["development", "published"], ["development"]
    ]:
        raise RuntimeError("Only publish=true dispatches may add the published provider")
    events = workflow.get("on", workflow.get(True, {}))  # PyYAML's YAML 1.1 'on'.
    inputs = events["workflow_dispatch"]["inputs"]
    if "published_najaeda" in inputs or inputs["publish"].get("type") != "boolean":
        raise RuntimeError("The boolean publish input must control provider selection")
    rows = matrix["include"]
    if (sorted(matrix["os"]) != sorted(row["os"] for row in rows)
            or len(set(matrix["os"])) != len(rows)
            or matrix.get("exclude") or any("provider" in row for row in rows)):
        raise RuntimeError("Each OS must have exactly one provider-independent platform row")
    if build.get("continue-on-error", False):
        raise RuntimeError("Both provider test matrices must succeed before publication")
    if build["env"]["KEPLER_USE_PUBLISHED_NAJAEDA"] != (
        "${{ matrix.provider == 'published' && '1' || '0' }}"
    ):
        raise RuntimeError("Each matrix provider must select its own build mode")
    uploaded = [
        step["with"]["name"] for step in build["steps"]
        if step.get("uses", "").startswith("actions/upload-artifact@")
    ]
    if uploaded != [
        "kepler-formal-${{ matrix.provider }}-${{ matrix.platform }}-${{ matrix.arch }}"
    ]:
        raise RuntimeError("Wheel artifacts must distinguish providers and platforms")
    artifacts = {f"kepler-formal-published-{row['platform']}-{row['arch']}" for row in rows}
    publisher = workflow["jobs"]["publish"]
    downloaded = [
        step["with"]["name"] for step in publisher["steps"]
        if step.get("uses", "").startswith("actions/download-artifact@")
    ]
    if (set(downloaded) != artifacts or len(downloaded) != len(rows)
            or publisher["needs"] not in ("build", ["build"])):
        raise RuntimeError("Publisher must await both providers and download only published wheels")
    # No status override (such as always()) may bypass failed development tests.
    if {part.strip() for part in publisher["if"].split("&&")} != {
        "github.event_name == 'workflow_dispatch'", "inputs.publish",
        "github.repository == 'keplertech/kepler-formal'", "github.ref == 'refs/heads/main'",
    }:
        raise RuntimeError("Publication requires a successful release dispatch on upstream main")
    return artifacts


def main() -> None:
    import yaml

    root = Path(__file__).resolve().parents[1]
    workflow = yaml.safe_load(
        (root / ".github/workflows/python-wheels.yml").read_text(encoding="utf-8")
    )
    naja_workflow = yaml.safe_load(
        (root / "thirdparty/naja/.github/workflows/wheels.yml").read_text(
            encoding="utf-8"
        )
    )
    expected = {
        f"cp{row['python']}-{row['platform_id']}"
        for row in naja_workflow["jobs"]["build_wheels"]["strategy"]["matrix"]["include"]
    }
    platforms = {"manylinux_2_28": "linux", "macOS": "macos", "Windows": "windows"}
    # Compare the checked-in configuration, not a caller's local CIBW overrides.
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("CIBW_")}
    actual: set[str] = set()
    artifacts = check_release_jobs(workflow)
    for row in workflow["jobs"]["build"]["strategy"]["matrix"]["include"]:
        completed = subprocess.run(
            [sys.executable, "-m", "cibuildwheel", "--print-build-identifiers",
             "--platform", platforms[row["platform"]], "--archs", row["arch"]],
            cwd=root, env=environment, check=True, capture_output=True, text=True,
        )
        identifiers = set(completed.stdout.split())
        if not identifiers or actual.intersection(identifiers):
            raise RuntimeError(f"Empty or duplicate wheel selection: {row}")
        actual.update(identifiers)
    if actual != expected:
        raise RuntimeError(
            f"Wheel matrix differs from NajaEDA. Missing: {sorted(expected - actual)}; "
            f"extra: {sorted(actual - expected)}"
        )
    print(f"Wheel matrix matches NajaEDA: {len(actual)} identifiers, {len(artifacts)} platforms.")
    for identifier in sorted(actual):
        print(f"  {identifier}")


if __name__ == "__main__":
    main()
