# yaml_to_cli_demo

A minimal, self-contained reimplementation of the core ideas behind
[`edison/convert_lab_to_cli.py`](../edison/convert_lab_to_cli.py), using a
generic (server / user / network-interface) domain instead of DOCSIS
terminology. It has no dependency on any code in this repo - only PyYAML
and Jinja2.

Run it with:

```bash
cd yaml_to_cli_demo
python3 main.py -e dev -b full_setup
```

## The six core ideas

### 1. Commands are data, not code

Each CLI command is defined as a YAML file with a Jinja2 template string
inside it, under [`commands/`](commands/). For example
[`commands/create_server.yaml.j2`](commands/create_server.yaml.j2):

```yaml
version: "1.0"
commands: |
    server {{ name }}
      role {{ role | default('generic') }}
      network-pool {{ network_pool | default('default_pool') }}
      {% for tag in tags | default([]) -%}
      tag {{ tag }}
      {% endfor -%}
    !
```

This is the same trick as `edison/devices/vcmts/cli/create/general/base/create_vmc.yaml.j2`
(lines 12-49 of that file): the *content* of a command lives in a template
file, so adding a new command means adding a new file, not editing Python.

### 2. Templates are loaded once into a name-keyed registry

[`converter.py:Converter._load_commands`](converter.py) walks the
`commands/` directory, parses every `*.yaml.j2` with `yaml.safe_load`, and
stores it in `self.templates[(name, version)]`. `_get_template(name)` then
looks it up by name, exactly like `_load_commands` / `_get_command` in the
original script (`convert_lab_to_cli.py:106-154`).

### 3. Rendering and formatting are separate steps

`Converter.render()` does two things in sequence:

1. Jinja2 fills in the values (`Environment(loader=BaseLoader()).from_string(...).render(**env_data)`).
2. Two `re.sub` calls clean up whitespace afterwards (strip the YAML block
   scalar's indentation, collapse blank lines).

Template authors only worry about content; the converter guarantees
consistent layout. This mirrors `_render_command`
(`convert_lab_to_cli.py:156-176`).

### 4. Converting is a fixed-order pipeline of independent sections

`Converter.convert()` builds a list of `(section_name, [commands])` pairs
and concatenates them in order:

```python
sections = [
    ("standard_banner", self._process_static_command("standard_banner")),
    ("users", self._process_users(inventory)),
    ("servers", self._process_servers(inventory)),
]
if self._include_network_sections:
    sections.append(("network_interfaces", self._process_network_interfaces(inventory)))
```

Three flavors of section, matching three helper patterns in the original
`convert_testbed_to_cli` (`convert_lab_to_cli.py:181-244`):

| Section              | Pattern                                            | Original equivalent          |
|----------------------|-----------------------------------------------------|-------------------------------|
| `standard_banner`    | static, ignores the inventory entirely              | `_process_static_command`     |
| `users`              | one inventory item -> one rendered command          | `_process_simple_section`     |
| `servers`            | fills in defaults before rendering                  | `_process_vmcs` / `_prepare_vmc_data` |
| `network_interfaces` | associates two already-created objects, runs last   | `_process_assign_vmc_to_rpd`  |

The order is not arbitrary: `network_interfaces` refers to servers by
name, so it has to run after `servers` - the same dependency-ordering
concern that keeps `vmcs` before `assign_vmc_to_rpd` in the original
script.

### 5. Missing fields get sensible defaults from elsewhere in the inventory

`Converter._prepare_server_data` fills in `role` from the active
environment's default, and `network_pool` by picking the first pool
defined under `address_pools.network` if the server didn't specify one:

```python
if "network_pool" not in data:
    pools = inventory.get("address_pools", {}).get("network", {})
    if pools:
        data["network_pool"] = next(iter(pools))
```

This is the same "search elsewhere for a plausible value" pattern as
`_prepare_vmc_data`'s `ssi_v4` / `ssi_v6` / `nsi_v4` / `nsi_v6` lookups
(`convert_lab_to_cli.py:601-621`) - it lets the input YAML stay small
because the converter, not the user, does the cross-referencing.

### 6. Environment and batch are two independent axes

* **Environment** (`-e dev` / `-e prod`) selects *which inventory file to
  load* and *which defaults apply* (e.g. `prod`'s default server role is
  `hardened`, `dev`'s is `generic`).
* **Batch** (`-b quick_setup` / `-b full_setup`) selects *which sections
  are even included* - `quick_setup` skips `network_interfaces`
  entirely.

Either one can be omitted and gets derived from the other via
`DEFAULT_ENVIRONMENT_BY_BATCH` / `DEFAULT_BATCH_BY_ENVIRONMENT`, and if
both are omitted there's a hardcoded fallback pair. This is a smaller
version of `_resolve_environment_and_batch`
(`convert_lab_to_cli.py:744-771`).

### 7. Output assembly

`build_output_text` wraps the rendered commands in a `config` / `commit`
CLI session, optionally naming each command with a comment
(`--include-names`) or committing after every section instead of once at
the end (`--commit-after-sections`) - the same knobs as
`build_lab_cli_output_text` (`convert_lab_to_cli.py:787-804`).

## Files

```
yaml_to_cli_demo/
  README.md                          this file
  converter.py                       Converter class + env/batch resolution + output assembly
  main.py                            argparse CLI entrypoint
  commands/
    standard_banner.yaml.j2          static section example
    create_user.yaml.j2              simple list section example
    create_server.yaml.j2            default-filling section example
    create_network_interface.yaml.j2 association section example
  inventory/
    dev.yaml                         example inventory: all sections have data
    prod.yaml                        example inventory: smaller, still works unchanged
```

## Try it yourself

```bash
# Full pipeline, dev inventory, all sections included
python3 main.py -e dev -b full_setup

# Same inventory, but skip the network_interfaces section
python3 main.py -e dev -b quick_setup

# Only -b given -> environment is inferred (full_setup -> prod)
python3 main.py -b full_setup

# Only -e given -> batch is inferred (prod -> full_setup)
python3 main.py -e prod

# Show which section produced which command
python3 main.py -e dev -b full_setup --include-names

# Commit after each section instead of once at the end
python3 main.py -e dev -b full_setup --commit-after-sections

# Unknown environment/batch fails fast with a clear message
python3 main.py -e staging
```
