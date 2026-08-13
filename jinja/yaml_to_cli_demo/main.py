#!/usr/bin/env python3
"""CLI entrypoint for the yaml_to_cli_demo converter.

Mirrors the argparse layout of edison/convert_lab_to_cli.py's main(), but
scoped down to this demo's generic (-e environment / -b batch) inventory.
"""

from __future__ import annotations

import argparse
import os
import sys

import yaml

from converter import Converter, build_output_text, resolve_environment_and_batch

HERE = os.path.dirname(os.path.abspath(__file__))
COMMANDS_DIR = os.path.join(HERE, "commands")
INVENTORY_DIR = os.path.join(HERE, "inventory")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert a demo inventory YAML to CLI text (-e environment, -b batch)"
    )
    parser.add_argument(
        "-e",
        "--environment",
        help="Environment name (matches inventory/<name>.yaml unless --inventory-path is given)",
    )
    parser.add_argument(
        "-b",
        "--batch",
        help="Batch profile (quick_setup, full_setup); controls which sections are included",
    )
    parser.add_argument(
        "--inventory-path",
        help="Path to an inventory YAML (overrides the file inferred from -e)",
    )
    parser.add_argument("-o", "--output", help="Output file path (default: stdout)")
    parser.add_argument(
        "--include-names",
        action="store_true",
        help="Include command names as comments before each command",
    )
    parser.add_argument(
        "--commit-after-sections",
        action="store_true",
        help="Add a commit statement after each section instead of once at the end",
    )

    args = parser.parse_args()

    environment, batch = resolve_environment_and_batch(args.environment, args.batch)

    if args.inventory_path:
        inventory_path = args.inventory_path
    else:
        inventory_path = os.path.join(INVENTORY_DIR, f"{environment}.yaml")

    if not os.path.exists(inventory_path):
        print(f"Error: inventory file not found: {inventory_path}", file=sys.stderr)
        sys.exit(1)

    with open(inventory_path) as f:
        inventory = yaml.safe_load(f) or {}

    converter = Converter(COMMANDS_DIR, environment=environment, batch=batch)
    cli_commands = converter.convert(inventory, commit_after_sections=args.commit_after_sections)
    output_text = build_output_text(
        cli_commands,
        include_names=args.include_names,
        commit_after_sections=args.commit_after_sections,
    )

    if args.output:
        with open(args.output, "w") as f:
            f.write(output_text)
    else:
        sys.stdout.write(output_text)
        if output_text:
            sys.stdout.write("\n")


if __name__ == "__main__":
    main()
