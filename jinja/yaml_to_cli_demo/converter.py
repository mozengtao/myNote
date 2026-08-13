"""Minimal, self-contained YAML -> Jinja2 -> CLI converter.

This module distills the core mental model of edison/convert_lab_to_cli.py
into a small, dependency-free example:

1. CLI commands are *data* (YAML files with a Jinja2 template string inside),
   not Python code. Adding a new command means adding a new template file,
   not touching converter.py.
2. All templates are loaded once into a registry keyed by (name, version),
   then looked up by name when a section needs to render a command.
3. Rendering and formatting are two separate steps: Jinja2 only fills in
   values; a couple of regexes afterwards normalize whitespace so template
   authors don't have to hand-tune indentation for every case.
4. Converting an inventory to CLI is a fixed-order pipeline of independent
   "sections". Each section reads one part of the inventory, builds a
   dict of template variables ("env data"), renders it, and returns
   (command_name, command_text) pairs. Order encodes dependency order:
   static setup first, associations between objects last.
5. Missing fields are filled in with sensible defaults derived from other
   parts of the inventory, so the input YAML can stay small.
6. Environment + batch are two independent axes: environment selects which
   defaults apply, batch selects which sections are even included.
"""

from __future__ import annotations

import os
import re
import sys

import yaml
from jinja2 import Environment, BaseLoader


# ---------------------------------------------------------------------------
# Environment / batch strategy (see README section 6 for the full picture).
# These are intentionally tiny dicts, but they play the same role as
# ENVIRONMENT_TESTBED_STEM / DEFAULT_ENVIRONMENT_BY_BATCH / BATCH_CONFIG in
# the original script: two independent knobs that can each default the other.
# ---------------------------------------------------------------------------

ENVIRONMENTS: dict[str, dict] = {
    "dev": {"default_role": "generic"},
    "prod": {"default_role": "hardened"},
}

BATCHES: dict[str, dict] = {
    "quick_setup": {"include_network_sections": False},
    "full_setup": {"include_network_sections": True},
}

DEFAULT_ENVIRONMENT_BY_BATCH: dict[str, str] = {
    "quick_setup": "dev",
    "full_setup": "prod",
}

DEFAULT_BATCH_BY_ENVIRONMENT: dict[str, str] = {
    "dev": "full_setup",
    "prod": "full_setup",
}


def resolve_environment_and_batch(
    environment: str | None, batch: str | None
) -> tuple[str, str]:
    """Fill in whichever of (environment, batch) was omitted.

    Mirrors the three-way branching in convert_lab_to_cli.py's
    _resolve_environment_and_batch: both missing -> hardcoded default pair;
    one missing -> derive it from the other; both given -> just validate.
    """
    if environment is None and batch is None:
        return "dev", "quick_setup"

    if environment is None:
        assert batch is not None
        _require_known_batch(batch)
        environment = DEFAULT_ENVIRONMENT_BY_BATCH.get(batch, "dev")
        return environment, batch

    if batch is None:
        assert environment is not None
        _require_known_environment(environment)
        batch = DEFAULT_BATCH_BY_ENVIRONMENT.get(environment, "quick_setup")
        return environment, batch

    _require_known_environment(environment)
    _require_known_batch(batch)
    return environment, batch


def _require_known_environment(environment: str) -> None:
    if environment not in ENVIRONMENTS:
        known = ", ".join(sorted(ENVIRONMENTS))
        print(f"Error: unknown environment {environment!r}. Known: {known}", file=sys.stderr)
        sys.exit(1)


def _require_known_batch(batch: str) -> None:
    if batch not in BATCHES:
        known = ", ".join(sorted(BATCHES))
        print(f"Error: unknown batch {batch!r}. Known: {known}", file=sys.stderr)
        sys.exit(1)


# ---------------------------------------------------------------------------
# The converter itself.
# ---------------------------------------------------------------------------


class Converter:
    """Loads command templates and turns an inventory dict into CLI text."""

    def __init__(self, commands_path: str, *, environment: str, batch: str):
        self.commands_path = commands_path
        self.environment = environment
        self.batch = batch
        self._default_role = ENVIRONMENTS[environment]["default_role"]
        self._include_network_sections = BATCHES[batch]["include_network_sections"]

        self.templates: dict[tuple[str, str], dict] = {}
        self.j2env = Environment(loader=BaseLoader())
        self._load_commands()

    # -- template registry -------------------------------------------------

    def _load_commands(self) -> None:
        """Scan commands_path for *.yaml.j2 and register each one by name."""
        if not os.path.isdir(self.commands_path):
            print(f"Warning: commands path does not exist: {self.commands_path}", file=sys.stderr)
            return

        for root, _dirs, files in os.walk(self.commands_path):
            for file_name in files:
                if file_name.endswith("yaml.j2"):
                    self._load_one_template(os.path.join(root, file_name))

    def _load_one_template(self, file_path: str) -> None:
        with open(file_path) as f:
            try:
                template = yaml.safe_load(f)
            except yaml.YAMLError as ex:
                print(f"Failed to parse template {file_path}: {ex}", file=sys.stderr)
                return

        template.setdefault("version", "1.0")
        name = os.path.basename(file_path).split(".")[0]
        key = (name, template["version"])

        if key in self.templates:
            print(f"Warning: overwriting template {key}", file=sys.stderr)
        self.templates[key] = template

    def _get_template(self, name: str, version: str = "1.0") -> dict:
        template = self.templates.get((name, version))
        if template is None:
            # Version-agnostic fallback: pick any template with a matching name.
            for (tmpl_name, _tmpl_version), tmpl in self.templates.items():
                if tmpl_name == name:
                    template = tmpl
                    break
        if template is None:
            available = sorted({n for n, _v in self.templates})
            raise KeyError(f"Command {name!r} not found. Available: {available}")
        return template

    # -- rendering -----------------------------------------------------------

    def render(self, name: str, env_data: dict, version: str = "1.0") -> str:
        """Render one command template with env_data, then normalize whitespace.

        Rendering and formatting are deliberately two separate steps (see
        module docstring point 3): the template only deals with content, the
        regexes below deal with layout so every template doesn't need to
        hand-manage indentation consistently.
        """
        template = self._get_template(name, version)
        commands_template = template.get("commands", "")
        if not commands_template:
            return ""

        rendered = self.j2env.from_string(commands_template).render(**env_data).strip()

        if not template.get("preserve_indent", False):
            # Strip the 2+ space indent that comes from writing the template
            # nested inside a YAML block scalar; keep single-space CLI indent.
            rendered = re.sub(r"\n {2,}", "\n", rendered)
        rendered = re.sub(r"\n{2,}", "\n", rendered)
        return rendered.rstrip("\n")

    # -- sections --------------------------------------------------------

    def convert(
        self, inventory: dict, *, commit_after_sections: bool = False
    ) -> list[tuple[str, str]]:
        """Turn an inventory dict into an ordered list of (name, cli_text).

        This is the fixed-order pipeline described in the module docstring
        point 4: each section is independent, order encodes dependencies.
        """
        sections: list[tuple[str, list[tuple[str, str]]]] = [
            ("standard_banner", self._process_static_command("standard_banner")),
            ("users", self._process_users(inventory)),
            ("servers", self._process_servers(inventory)),
        ]

        if self._include_network_sections:
            sections.append(("network_interfaces", self._process_network_interfaces(inventory)))

        cli_commands: list[tuple[str, str]] = []
        for section_name, section_commands in sections:
            cli_commands.extend(section_commands)
            if commit_after_sections and section_commands:
                cli_commands.append((f"commit_{section_name}", "commit"))
        return cli_commands

    def _process_static_command(self, command_name: str) -> list[tuple[str, str]]:
        """A section with no inputs at all: it always renders the same text."""
        try:
            result = self.render(command_name, {})
        except KeyError:
            print(f"Warning: command {command_name} not found, skipping", file=sys.stderr)
            return []
        return [(command_name, result)] if result else []

    def _process_users(self, inventory: dict) -> list[tuple[str, str]]:
        """A simple list section: one inventory item -> one rendered command."""
        cli_commands: list[tuple[str, str]] = []
        for user in inventory.get("users", []):
            try:
                result = self.render("create_user", user)
            except Exception as e:
                print(f"Warning: failed to render create_user: {e}", file=sys.stderr)
                continue
            if result:
                cli_commands.append((f"create_user_{user.get('name', 'unknown')}", result))
        return cli_commands

    def _process_servers(self, inventory: dict) -> list[tuple[str, str]]:
        """A section that fills in defaults before rendering.

        Mirrors _prepare_vmc_data in the original script: a server that
        omits network_pool or role gets a value derived from elsewhere in
        the inventory (or from the active environment), so callers don't
        have to repeat boilerplate in every server entry.
        """
        cli_commands: list[tuple[str, str]] = []
        for server in inventory.get("servers", []):
            try:
                server_data = self._prepare_server_data(server, inventory)
                result = self.render("create_server", server_data)
            except Exception as e:
                print(f"Warning: failed to render create_server: {e}", file=sys.stderr)
                continue
            if result:
                cli_commands.append((f"create_server_{server_data['name']}", result))
        return cli_commands

    def _prepare_server_data(self, server: dict, inventory: dict) -> dict:
        data = server.copy()

        if "role" not in data:
            data["role"] = self._default_role

        if "network_pool" not in data:
            pools = inventory.get("address_pools", {}).get("network", {})
            if pools:
                # Pick the first configured pool name as a reasonable default,
                # the same "search elsewhere for a plausible value" pattern
                # the original script uses for ssi_v4/ssi_v6/nsi_v4/nsi_v6.
                data["network_pool"] = next(iter(pools))

        return data

    def _process_network_interfaces(self, inventory: dict) -> list[tuple[str, str]]:
        """An association section: only meaningful once servers already exist."""
        cli_commands: list[tuple[str, str]] = []
        for interface in inventory.get("network_interfaces", []):
            try:
                result = self.render("create_network_interface", interface)
            except Exception as e:
                print(f"Warning: failed to render create_network_interface: {e}", file=sys.stderr)
                continue
            if result:
                name = f"{interface.get('server', 'unknown')}_{interface.get('name', 'unknown')}"
                cli_commands.append((f"create_network_interface_{name}", result))
        return cli_commands


# ---------------------------------------------------------------------------
# Output assembly (see module docstring point 7).
# ---------------------------------------------------------------------------


def build_output_text(
    cli_commands: list[tuple[str, str]],
    *,
    include_names: bool = False,
    commit_after_sections: bool = False,
) -> str:
    """Wrap the rendered commands in a config/commit CLI session."""
    lines: list[str] = []
    if cli_commands:
        lines.append("config")
    for name, text in cli_commands:
        if include_names:
            lines.append(f"# Command: {name}")
        lines.append(text)
    if cli_commands and not commit_after_sections:
        lines.append("commit")
    return "\n".join(lines)
