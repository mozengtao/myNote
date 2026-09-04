#!/usr/bin/env python3

import argparse
from pathlib import Path

import yaml
from jinja2 import Environment, FileSystemLoader, StrictUndefined


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate EVC CLI commands from YAML configuration"
    )

    parser.add_argument(
        "-c",
        "--config",
        default="config/images.yaml",
        help="YAML configuration file",
    )

    parser.add_argument(
        "-t",
        "--template",
        default="templates/evc.cli.j2",
        help="Jinja2 template file",
    )

    parser.add_argument(
        "-o",
        "--output",
        default="output/evc.cli",
        help="Output file",
    )

    return parser.parse_args()


def load_config(filename):
    with open(filename, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def validate_config(config):
    images = config.get("images", {})

    for name, image in images.items():
        if not image.get("path"):
            raise ValueError(f"{name}: path is required")

        if not image.get("version"):
            raise ValueError(f"{name}: version is required")


def render_template(config, filename):
    template_path = Path(filename)

    env = Environment(
        loader=FileSystemLoader(template_path.parent),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    template = env.get_template(template_path.name)

    return template.render(**config)


def save_output(content, filename):
    output_path = Path(filename)

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    output_path.write_text(
        content,
        encoding="utf-8",
    )


def main():
    args = parse_args()

    config = load_config(args.config)
    validate_config(config)

    output = render_template(
        config,
        args.template,
    )

    save_output(
        output,
        args.output,
    )

    print(f"Generated: {args.output}")


if __name__ == "__main__":
    main()