import yaml
from jinja2 import Environment, FileSystemLoader

env = Environment(loader=FileSystemLoader('.'),
                  trim_blocks=True,
                  lstrip_blocks=True)

template = env.get_template('template.j2')

with open ('vars.yaml', 'r') as f:
    data = yaml.safe_load(f)

config = template.render(data)
with open ('config.txt', 'w') as fw:
    fw.write(config)
