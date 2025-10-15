[Python Tips](https://book.pythontips.com/en/latest/index.html)  
[Python Like You Mean It](https://www.pythonlikeyoumeanit.com/index.html)  
[Python Tutorial | Learn Python Language](https://www.wscubetech.com/resources/python)  
[How to Learn Python (Step-by-Step)](https://www.dataquest.io/blog/learn-python-the-right-way/)  
[Python 101](https://python101.pythonlibrary.org/index.html)  
[**Learn Python Programming**](https://www.programiz.com/python-programming)  
[]()  
[]()  
[**Python Formatter**](https://codebeautify.org/python-formatter-beautifier) #online  
[]()  
[Data Classes in Python 3.7+ (Guide)](https://realpython.com/python-data-classes/)  
[Python Standard Library](https://realpython.com/ref/stdlib/)  
[Python's property(): Add Managed Attributes to Your Classes](https://realpython.com/python-property/)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  

## common utility example
cfg.toml:
```toml
[paths]
work_dir = "/tmp"

[names]
evc_job = "evc-morris-dentist"
vmc_job = "vmc-morris-dentist"

[defaults]
force = true
```
x.py:
```python
#!/usr/bin/env python3

import sys
import shlex
import argparse
import subprocess
from pathlib import Path
from pprint import pprint

try:
    import tomllib
except ImportError:
    import tomli as tomllib

class Config:
    def __init__(self, data: dict):
        self.paths = data.get("paths", {})
        self.names = data.get("names", {})
        self.defaults = data.get("defaults", {})
    
    @property
    def work_dir(self) -> str:
        return self.paths.get("work_dir", ".")

    @property
    def evc_job(self) -> str:
        return self.names.get("evc_job", "evc-job")

    @property
    def vmc_job(self) -> str:
        return self.names.get("vmc_job", "vmc-job")

    @property
    def force(self) -> bool:
        return self.defaults.get("force", True)

def load_config(config_path: str) -> Config:
    cfg_path = Path(config_path)
    if not cfg_path.exists():
        raise FileNotFoundError(f"Config file not found: {cfg_path}")
    with open(cfg_path, "rb") as f:
        data = tomllib.load(f)
    return Config(data)

def run(cmd: str, cwd: str | None = None, capture: bool = False) -> subprocess.CompletedProcess:
    kwargs = {
        "shell": True,
        "cwd": cwd,
    }
    if capture:
        kwargs.update({
            "stdout": subprocess.PIPE,
            "stderr": subprocess.PIPE,
            "text": True,
        })
    #return subprocess.run(cmd, **kwargs)
    # for test only
    print(cmd)
    res = subprocess.run("ls", **kwargs)
    return res

def run_check_output(cmd: str, cwd: str | None = None) -> str:
    res = run(cmd, cwd=cwd, capture=True)
    if res.returncode != 0:
        raise subprocess.CalledProcessError(f"Command failed: {cmd}\nstdout: {res.stdout}\nstderr: {res.stderr}")
    return res.stdout

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Nomad helper")
    subparsers = parser.add_subparsers(dest="cmd")

    # start
    ps = subparsers.add_parser("start", help="Start EVC or VMC")
    ps.add_argument("job", choices=["evc", "vmc"], help="Job to start")
    ps.add_argument("--config", default="cfg.toml", help="Config file")

    # terminal
    pt = subparsers.add_parser("terminal", help="Terminal EVC or VMC")
    pt.add_argument("job", choices=["evc", "vmc"], help="Job to terminal")
    pt.add_argument("--config", default="cfg.toml", help="Config file")

    # purge
    ps = subparsers.add_parser("purge", help="Purge EVC or VMC")
    ps.add_argument("job", choices=["evc", "vmc"], help="Job to purge")
    ps.add_argument("--config", default="cfg.toml", help="Config file")

    return parser

def start_evc(cfg: Config) -> int:
    cwd = cfg.work_dir
    evc_cmd = (
        "nomad job run ",
        f"-var \"force={str(cfg.force).lower()}\" ",
        f"{cfg.evc_job}"
    )
    res = run(evc_cmd, cwd=cwd)
    return res.returncode

def run_evc_cli(cfg: Config, command: str) -> str:
    task = "evc"
    job = cfg.evc_job
	cmd = (
		f"nomad alloc exec -task {shlex.quote(task)} -job {shlex.quote(job)} sh -c '"
		f"ncs_cli -u admin <<EOF\n{command}\nEOF'"
	)
    return run_check_output(cmd)

def start_vmc(cfg: Config) -> int:
    cwd = cfg.work_dir
    vmc_cmd = (
        "nomad job run ",
        f"-var \"force={str(cfg.force).lower()}\" ",
        f"{cfg.vmc_job}"
    )
    res = run(vmc_cmd, cwd=cwd)
    return res.returncode

def terminal_evc(cfg: Config) -> int:
    job = cfg.evc_job
    task = "evc"
    cmd = f"nomad alloc exec -task {shlex.quote(task)} -job {shlex.quote(job)} sh"
    res = run(cmd, cwd=cfg.work_dir)
    return res.returncode

def terminal_vmc(cfg: Config) -> int:
    job = cfg.vmc_job
    task = "vmc"
    cmd = f"nomad alloc exec -task {shlex.quote(task)} -job {shlex.quote(job)} sh"
    res = run(cmd, cwd=cfg.work_dir)
    return res.returncode

def purge_evc(cfg: Config) -> int:
    cwd = cfg.work_dir
    evc_cmd = (
        "nomad job stop ",
        f"{cfg.evc_job}"
    )
    return run(evc_cmd, cwd=cwd).returncode


def purge_vmc(cfg: Config) -> int:
    cwd = cfg.work_dir
    vmc_cmd = (
        "nomad job stop ",
        f"{cfg.vmc_job}"
    )
    return run(vmc_cmd, cwd=cwd).returncode

def main(argv: list[str]) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    cfg = load_config(getattr(args, "config", "cfg.toml"))
```
    if args.cmd == "start":
        if args.job == "evc":
            start_evc(cfg)
        elif args.job == "vmc":
            start_vmc(cfg)
    elif args.cmd == "terminal":
        if args.job == "evc":
            terminal_evc(cfg)
        elif args.job == "vmc":
            terminal_vmc(cfg)
    elif args.cmd == "purge":
        if args.job == "evc":
            purge_evc(cfg)
        elif args.job == "vmc":
            purge_vmc(cfg)
    else:
        parser.print_help()
        return 1
```
    match args.cmd:
        case "start":
            match args.job:
                case "evc":
                    start_evc(cfg)
                case "vmc":
                    start_vmc(cfg)
                case _:
                    parser.print_help()
                    return 1
        case "terminal":
            match args.job:
                case "evc":
                    terminal_evc(cfg)
                case "vmc":
                    terminal_vmc(cfg)
                case _:
                    parser.print_help()
                    return 1
        case "purge":
            match args.job:
                case "evc":
                    purge_evc(cfg)
                case "vmc":
                    purge_vmc(cfg)
                case _:
                    parser.print_help()
                    return 1
        case _:
            parser.print_help()
            return 1

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
```

## cli example
config file:
```toml
[paths]
work_dir = "/mnt/DATA/morrism/testbed/gen0114"

[names]
# EVC job name used for exec into controller container
evc_job = "evc-morris-dentist"
# Prefix used to parse VMC job name from EVC CLI output
vmc_prefix = "vmc-morris-dentist"
# Used to find the running alloc for vmc logs via `nomad status`
vmc_evc_combined = "vmc-evc-morris-dentist"
# SNMP job name queried by `nomad job allocs`
snmp_job = "snmp-evc-morris-dentist-1"

[defaults]
force = true

[evc]
host_constraint = "dev5"
image_path = "artifactory.cn.vecima.com/docker/jenkins/evc/nightly:latest"
hcl_path = "morris-dentist_evc_container.nomad.hcl"

[kafka]
service_name_prefix = "morris-dentist"
image_path = "artifactory.cn.vecima.com/docker/jenkins/kafka/nightly:latest"
hcl_path = "morris-dentist_kafka_container.nomad.hcl"

[ksqldb]
service_name = "ksqldb-morris-dentist"
image_path = "artifactory.cn.vecima.com/docker/jenkins/ksqldb/nightly:latest"
hcl_path = "morris-dentist_ksqldb_container.nomad.hcl"
kafka_service_name = "morris-dentist-kafka-bootstrap-server"

[logs]
# Number of leading characters to strip from lines in `nomad alloc logs` output
strip_prefix_chars = 45
```
python file:
```python
#!/usr/bin/env python3
import argparse
import subprocess
import sys
import os
import shlex
from pathlib import Path

try:
	import tomllib  # Python 3.11+
except ModuleNotFoundError:
	import tomli as tomllib  # fallback for <3.11


class Config:
	def __init__(self, data: dict):
		self.paths = data.get("paths", {})
		self.names = data.get("names", {})
		self.defaults = data.get("defaults", {})
		self.evc = data.get("evc", {})
		self.kafka = data.get("kafka", {})
		self.ksqldb = data.get("ksqldb", {})
		self.logs = data.get("logs", {})

	@property
	def work_dir(self) -> str:
		return self.paths.get("work_dir", ".")

	@property
	def evc_job(self) -> str:
		return self.names.get("evc_job", "evc-morris-dentist")

	@property
	def vmc_prefix(self) -> str:
		return self.names.get("vmc_prefix", "vmc-morris-dentist")

	@property
	def vmc_evc_combined(self) -> str:
		return self.names.get("vmc_evc_combined", "vmc-evc-morris-dentist")

	@property
	def snmp_job(self) -> str:
		return self.names.get("snmp_job", "snmp-evc-morris-dentist-1")

	@property
	def force(self) -> bool:
		return bool(self.defaults.get("force", True))

	@property
	def evc_host_constraint(self) -> str:
		return self.evc.get("host_constraint", "dev5")

	@property
	def evc_image_path(self) -> str:
		return self.evc.get("image_path", "")

	@property
	def evc_hcl_path(self) -> str:
		return self.evc.get("hcl_path", "")

	@property
	def kafka_service_name_prefix(self) -> str:
		return self.kafka.get("service_name_prefix", "")

	@property
	def kafka_image_path(self) -> str:
		return self.kafka.get("image_path", "")

	@property
	def kafka_hcl_path(self) -> str:
		return self.kafka.get("hcl_path", "")

	@property
	def ksqldb_service_name(self) -> str:
		return self.ksqldb.get("service_name", "")

	@property
	def ksqldb_image_path(self) -> str:
		return self.ksqldb.get("image_path", "")

	@property
	def ksqldb_hcl_path(self) -> str:
		return self.ksqldb.get("hcl_path", "")

	@property
	def kafka_bootstrap_service_name(self) -> str:
		return self.ksqldb.get("kafka_service_name", "")

	@property
	def log_strip_prefix_chars(self) -> int:
		return int(self.logs.get("strip_prefix_chars", 45))


def load_config(config_path: str) -> Config:
	cfg_path = Path(config_path)
	if not cfg_path.exists():
		raise FileNotFoundError(f"Config TOML not found: {cfg_path}")
	with cfg_path.open("rb") as f:
		data = tomllib.load(f)
	return Config(data)


def run(cmd: str, cwd: str | None = None, capture: bool = False) -> subprocess.CompletedProcess:
	kwargs = {
		"shell": True,
		"cwd": cwd,
	}
	if capture:
		kwargs.update({"stdout": subprocess.PIPE, "stderr": subprocess.PIPE, "text": True})
	return subprocess.run(cmd, **kwargs)


def run_check_output(cmd: str, cwd: str | None = None) -> str:
	res = run(cmd, cwd=cwd, capture=True)
	if res.returncode != 0:
		raise RuntimeError(f"Command failed: {cmd}\nstdout:\n{res.stdout}\nstderr:\n{res.stderr}")
	return res.stdout


def run_ncs_command(cfg: Config, command: str) -> str:
	task = "evc"
	job = cfg.evc_job
	cmd = (
		f"nomad alloc exec -task {shlex.quote(task)} -job {shlex.quote(job)} sh -c '"
		f"ncs_cli -u admin <<EOF\n{command}\nEOF'"
	)
	return run_check_output(cmd)


def terminal(cfg: Config, task: str) -> int:
	if task == "evc":
		job = cfg.evc_job
		cmd = f"nomad alloc exec -task {shlex.quote(task)} -job {shlex.quote(job)} sh"
		return run(cmd).returncode
	elif task == "vmc":
		output = run_ncs_command(cfg, "show vmc status | t | nomore")
		job = ""
		for line in output.splitlines():
			if line.startswith(cfg.vmc_prefix):
				parts = line.split()
				if len(parts) >= 2:
					job = parts[1]
					break
		if not job:
			print("unknown container type (vmc job not found)")
			return 1
		cmd = f"nomad alloc exec -task {shlex.quote(task)} -job {shlex.quote(job)} sh"
		return run(cmd).returncode
	else:
		print("unknown container type")
		return 1


def show_snmp_nsi_port(cfg: Config) -> int:
	job = cfg.snmp_job
	allocs_cmd = f"nomad job allocs {shlex.quote(job)}"
	try:
		out = run_check_output(allocs_cmd)
	except RuntimeError as e:
		print(e)
		return 1
	alloc_id = ""
	for line in out.splitlines():
		if "snmp" in line:
			alloc_id = line.split()[0]
			break
	if not alloc_id:
		return 1
	status_cmd = f"nomad alloc status {shlex.quote(alloc_id)} 2>/dev/null | awk '/snmp-nsi-port/{{print $3}}'"
	res = run(status_cmd, capture=True)
	if res.stdout:
		print(res.stdout.strip())
	return 0 if res.returncode == 0 else res.returncode


def start_event(cfg: Config) -> int:
	cwd = cfg.work_dir
	print("starting kafka container")
	k_cmd = (
		"nomad job run "
		f"-var \"force={str(cfg.force).lower()}\" "
		f"-var \"kafka_service_name_prefix={cfg.kafka_service_name_prefix}\" "
		f"-var \"image_path={cfg.kafka_image_path}\" "
		f"{cfg.kafka_hcl_path} > /dev/null 2>&1 &"
	)
	run(k_cmd, cwd=cwd)

	print("starting ksqldb container")
	ks_cmd = (
		"nomad job run "
		f"-var \"force={str(cfg.force).lower()}\" "
		f"-var \"kafka_service_name={cfg.kafka_bootstrap_service_name}\" "
		f"-var \"ksqldb_service_name={cfg.ksqldb_service_name}\" "
		f"-var \"image_path={cfg.ksqldb_image_path}\" "
		f"{cfg.ksqldb_hcl_path} > /dev/null 2>&1 &"
	)
	run(ks_cmd, cwd=cwd)

	print("starting evc container")
	evc_cmd = (
		"nomad job run "
		f"-var \"force={str(cfg.force).lower()}\" "
		f"-var \"host_constraint={cfg.evc_host_constraint}\" "
		f"-var \"image_path={cfg.evc_image_path}\" "
		f"-var \"kafka_service_name={cfg.kafka_bootstrap_service_name}\" "
		f"-var \"ksqldb_service_name={cfg.ksqldb_service_name}\" "
		f"{cfg.evc_hcl_path} > /dev/null 2>&1 &"
	)
	run(evc_cmd, cwd=cwd)

	# wait for background jobs
	# There is no simple job control via subprocess when using shell=True & background, so we sleep briefly
	# to give jobs time to submit, mirroring the bash `wait` not strictly necessary here.
	print("Started kafka, ksqldb and evc container")
	return 0


def start_evc(cfg: Config) -> int:
	cwd = cfg.work_dir
	evc_cmd = (
		"nomad job run "
		f"-var \"force={str(cfg.force).lower()}\" "
		f"-var \"host_constraint={cfg.evc_host_constraint}\" "
		f"-var \"image_path={cfg.evc_image_path}\" "
		f"{cfg.evc_hcl_path}"
	)
	res = run(evc_cmd, cwd=cwd)
	print("Starting VMC Container")
	return res.returncode


def purge_all() -> int:
	cmd = "nomad job stop -purge $(nomad status | grep dentist | awk '{print $1}' | xargs)"
	return run(cmd).returncode


def show_log(cfg: Config, container: str) -> int:
	if container != "vmc":
		print("Unkonwn container name")
		return 1
	cmd = (
		"nomad alloc logs -f -tail -task vmc -job $(nomad status | grep "
		f"{cfg.vmc_evc_combined} | awk '/running/{{print $1}}')"
	)
	# stream and trim prefix characters similar to awk substr
	with subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as proc:
		assert proc.stdout is not None
		strip_n = cfg.log_strip_prefix_chars
		for line in proc.stdout:
			line = line.rstrip("\n")
			if len(line) > strip_n:
				print(line[strip_n:])
			else:
				print("")
		return proc.wait()


def build_parser() -> argparse.ArgumentParser:
	p = argparse.ArgumentParser(description="Nomad helper")
	subparsers = p.add_subparsers(dest="cmd", required=True)

	# start
	ps = subparsers.add_parser("start", help="start evc or event")
	ps.add_argument("container", choices=["evc", "event"], help="target to start")
	ps.add_argument("--config", default="/tmp/config.toml")

	# purge
	pp = subparsers.add_parser("purge", help="purge all")
	pp.add_argument("container", choices=["all"], help="target to purge")
	pp.add_argument("--config", default="/tmp/config.toml")

	# terminal
	pt = subparsers.add_parser("terminal", help="open container shell")
	pt.add_argument("container", choices=["evc", "vmc"], help="container type")
	pt.add_argument("--config", default="/tmp/config.toml")

	# log
	pl = subparsers.add_parser("log", help="stream container logs")
	pl.add_argument("container", choices=["vmc"], help="container type")
	pl.add_argument("--config", default="/tmp/config.toml")

	# show
	pw = subparsers.add_parser("show", help="show info")
	pw.add_argument("container", choices=["snmp"], help="item to show")
	pw.add_argument("--config", default="/tmp/config.toml")

	return p


def main(argv: list[str]) -> int:
	parser = build_parser()
	args = parser.parse_args(argv)

	cfg = load_config(getattr(args, "config"))

	if args.cmd == "start":
		if args.container == "evc":
			return start_evc(cfg)
		elif args.container == "event":
			return start_event(cfg)
	elif args.cmd == "purge":
		return purge_all()
	elif args.cmd == "terminal":
		return terminal(cfg, args.container)
	elif args.cmd == "log":
		return show_log(cfg, args.container)
	elif args.cmd == "show":
		if args.container == "snmp":
			return show_snmp_nsi_port(cfg)

	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))

```

## OOP
OOP is a paradigm that provides a means of structuring programs so that properties and behaviors are bundled into individual objects.
1.**encapsulation**:bundle data (attributes) and behaviors (methods) within a class to create a cohesive unit, it helps maintain data integrity and promotes modular, secure code
2.**inheritance**: allows a class to inherit properties and behaviors from another class, it helps reduce code duplication and promotes code reuse
3.**abstraction**: allows a class to hide complex details and expose only the necessary functionality, allowing developers to focus on what an object does rather than how it achieves its functionality
4.**polymorphism**: allows objects of different classes to be treated as if they were of the same class, it allows you to access attributes and methods on objects without needing to worry about their actual class
polymorphism: allows objects of different classes to be treated as if they were of the same class, it allows you to access attributes and methods on objects without needing to worry about their actual class
```python
class Dog:
    #  class attributes
    species = "Canis familiaris"

    # instance attributes: attributes created in .__init__()
    def __init__(self, name, age):
        self.name = name
        self.age = age

    # dunder method(such as __str__, __init__, __del__, ...)
    def __str__(self):
        return f"{self.name} is {self.age} years old"

    # instance method
    def speak(self, sound):
        return f"{self.name} says {sound}"

# instantiating a class
Dog()   # <__main__.Dog object at 0x0004ccc90>

# inheritance
class Bulldog(Dog):
    def __init__(self, name, age, color):
        # access the parent class
        super().__init__(name, age)
        self.color = color

    # method overriding
    def speak(self, sound):
        return f"{self.name} says {sound} in {self.color} color"

jim = Bulldog("Jim", 5, "brown")
type(jim) # <class '__main__.Bulldog'>
isinstance(jim, Dog) # True
```

## property decorator
```python
# @property decorator
class Circle:
    def __init__(self, radius):
        print("__init__")
        self.radius = radius

    @property
    def radius(self):
        """The radius property"""
        print("Get radius")
        return self._radius
    
    @radius.setter
    def radius(self, value):
        print("Set radius")
        self._radius = value
    
    @radius.deleter
    def radius(self):
        print("Delete radius")
        del self._radius
    
    @property
    def area(self):
        print("Get area")
        return 3.14 * self.radius ** 2

circle = Circle(10)
circle.radius = 20
print(circle.radius)
del circle.radius
#print(circle.area) # error

# providing read-only attributes
class Point:
    def __init__(self, x, y):
        self._x = x
        self._y = y

    @property
    def x(self):
        return self._x

    @x.setter
    def x(self, value):
        raise WriteOnlyError("x is read-only")

    @property
    def y(self):
        return self._y

    @y.setter
    def y(self, value):
        raise WriteOnlyError("y is read-only")

point = Point(10, 20)
print(point.x)
point.x = 30 # error

# creating read-write attributes
class Circle:
    def __init__(self, radius):
        self.radius = radius

    @property
    def radius(self):
        return self._radius

    @radius.setter
    def radius(self, value):
        self._radius = float(value)

    @property
    def diameter(self):
        return self.radius * 2

    @diameter.setter
    def diameter(self, value):
        self.radius = value / 2

circle = Circle(42)
print(circle.radius)
print(circle.diameter)
circle.diameter = 100
print(circle.diameter)
print(circle.radius)

# providing write-only attributes
import hashlib
import os

class User:
    def __init__(self, name, password):
        self.name = name
        self.password = password

    @property
    def password(self):
        raise AttributeError("Password is write-only")

    @password.setter
    def password(self, plaintext):
        salt = os.urandom(32)
        self._hashed_password = hashlib.pbkdf2_hmac(
            "sha256", plaintext.encode("utf-8"), salt, 100_000
        )

john = User("John", "secret")
print(john._hashed_password)
john.password = "new_password"
print(john._hashed_password)

# example 1
class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    @property
    def x(self):
        return self._x

    @x.setter
    def x(self, value):
        try:
            self._x = float(value)
            print("Validated!")
        except ValueError:
            raise ValueError('"x" must be a number') from None

    @property
    def y(self):
        return self._y

    @y.setter
    def y(self, value):
        try:
            self._y = float(value)
            print("Validated!")
        except ValueError:
            raise ValueError('"y" must be a number') from None

point = Point(12, 5)
print(point.x)
print(point.y)
point.x = "twelve" # error
point.y = "five" # error

# example 1 inheritance
class Coordinate:
    def __set_name__(self, owner, name):
        self._name = name

    def __get__(self, instance, owner):
        return instance.__dict__[self._name]

    def __set__(self, instance, value):
        try:
            instance.__dict__[self._name] = float(value)
            print("Validated!")
        except ValueError:
            raise ValueError(f'"{self._name}" must be a number') from None

class Point:
    x = Coordinate()
    y = Coordinate()

    def __init__(self, x, y):
        self.x = x
        self.y = y

# example 2
import math

class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    @property
    def distance(self):
        return math.dist((0, 0), (self.x, self.y))

    @property
    def angle(self):
        return math.degrees(math.atan2(self.y, self.x))

    def as_cartesian(self):
        return self.x, self.y

    def as_polar(self):
        return self.distance, self.angle

point = Point(12, 5)
print(point.x)
print(point.y)
print(point.distance)
print(point.angle)
print(point.as_cartesian())
print(point.as_polar())

# caching computed attributes
from time import sleep

class Circle:
    def __init__(self, radius):
        self.radius = radius

    @property
    def radius(self):
        return self._radius

    @radius.setter
    def radius(self, value):
        self._diameter = None
        self._radius = value

    @property
    def diameter(self):
        if self._diameter is None:
            sleep(0.5)  # Simulate a costly computation
            self._diameter = self._radius * 2
        return self._diameter

circle = Circle(42.0)

print(circle.radius)
print(circle.diameter)  # With delay
print(circle.diameter)  # Without delay
circle.radius = 100.0
print(circle.diameter)  # With delay
print(circle.diameter)  # Without delay

# enhancement
from functools import cached_property
from time import sleep

class Circle:
    def __init__(self, radius):
        self.radius = radius

    @cached_property
    def diameter(self):
        sleep(0.5)  # Simulate a costly computation
        return self.radius * 2

# enhancement
from functools import cache
from time import sleep

class Circle:
    def __init__(self, radius):
        self.radius = radius

    @property
    @cache
    def diameter(self):
        sleep(0.5) # Simulate a costly computation
        return self.radius * 2

# Logging Attribute Access and Mutation
import logging

logging.basicConfig(
    format="%(asctime)s: %(message)s",
    level=logging.INFO,
    datefmt="%H:%M:%S"
)

class Circle:
    def __init__(self, radius):
        self._msg = '"radius" was %s. Current value: %s'
        self.radius = radius

    @property
    def radius(self):
        logging.info(self._msg % ("accessed", str(self._radius)))
        return self._radius

    @radius.setter
    def radius(self, value):
        try:
            self._radius = float(value)
            logging.info(self._msg % ("mutated", str(self._radius)))
        except ValueError:
            logging.info('validation error while mutating "radius"')

circle = Circle(42.0)
print(circle.radius)
circle.radius = 100
print(circle.radius)
circle.radius = "value" # error

# Managing Attribute Deletion
class TreeNode:
    def __init__(self, data):
        self._data = data
        self._children = []

    @property
    def children(self):
        return self._children

    @children.setter
    def children(self, value):
        if isinstance(value, list):
            self._children = value
        else:
            del self.children
            self._children.append(value)

    @children.deleter
    def children(self):
        self._children.clear()

    def __repr__(self):
        return f'{self.__class__.__name__}("{self._data}")'

root = TreeNode("root")
child1 = TreeNode("child 1")
child2 = TreeNode("child 2")
root.children = [child1, child2]
print(root.children)
del root.children
print(root.children)

#Creating Backward-Compatible Class APIs
```

## string
```python
cmd1 = "ls -l"
cmd2 = 'ls -l'
cmd3 = """ls -l"""      # '''ls -l'''
cmd4 = f"echo {2 + 3}"
cmd5 = r"echo C:\path\to\file"

# multiple-line string

#1 use triple-quoted strings for readability
cmd = f"""\
nomad alloc exec -task evc -job {EVCNAME} sh -c 'ncs_cli -u admin <<EOF
show cable modem brief | t
EOF' > {TMP_MODEM_LIST}
"""

#2 parentheses with implicit concatenation
cmd = (
    f"nomad alloc status $(nomad job allocs {EVCNAME} "
    "| awk '/snmp/{{print $1}}') 2>/dev/null "
    "| awk '/snmp-nsi-port/{{print $3}}'"
)

#3 for embeded templates
config_template = """\
interface {iface}
 description {desc}
 ip address {ip} {mask}
"""

print(config_template.format(iface="eth0", desc="uplink", ip="192.168.1.1", mask="255.255.255.0"))

# use template file

# get_modem_list.template:
nomad alloc exec -task evc -job {EVCNAME} sh -c 'ncs_cli -u admin <<EOF
show cable modem brief | t
EOF
' > {OUTPUT_FILE}
# load and fill the template in python
from pathlib import Path

TEMPLATE_PATH = Path("templates/get_modem_list.sh")

EVCNAME = "evc-morris-dentist"
TMP_MODEM_LIST = "/tmp/modem.list"

# Load template
template = TEMPLATE_PATH.read_text()

# Substitute variables
cmd = template.format(EVCNAME=EVCNAME, OUTPUT_FILE=TMP_MODEM_LIST)

print(cmd)

# one template file containing multiple command templates
#1
# modem_tasks.tpl:
### CMD:FETCH_MODEM_LIST
nomad alloc exec -task evc -job {EVCNAME} sh -c 'ncs_cli -u admin <<EOF
show cable modem brief | t
EOF
' > {OUTPUT_FILE}

### CMD:SNMP_WALK_MODEM
snmpwalk -v2c -c {COMMUNITY} {MODEM_IP} .1.3.6.1.2.1.2.2.1.2 > {OUTPUT_FILE}

### CMD:REBOOT_MODEM
snmpset -v2c -c {COMMUNITY} {MODEM_IP} .1.3.6.1.2.1.69.1.1.3.0 i 1

# python loader:
from pathlib import Path
import re

def load_templates(path):
    text = Path(path).read_text()
    sections = re.split(r'^### CMD:(\w+)', text, flags=re.M)
    # sections = ['', 'FETCH_MODEM_LIST', '<body>', 'SNMP_WALK_MODEM', '<body>', ...]
    templates = {}
    for i in range(1, len(sections), 2):
        templates[sections[i]] = sections[i+1].strip()
    return templates

templates = load_templates("templates/modem_tasks.sh.tpl")

cmd = templates["SNMP_WALK_MODEM"].format(
    COMMUNITY="public",
    MODEM_IP="192.168.0.2",
    OUTPUT_FILE="/tmp/snmp.out"
)

print(cmd)

#2
# modem_tasks.yaml:
FETCH_MODEM_LIST: |
  nomad alloc exec -task evc -job {EVCNAME} sh -c 'ncs_cli -u admin <<EOF
  show cable modem brief | t
  EOF
  ' > {OUTPUT_FILE}

SNMP_WALK_MODEM: |
  snmpwalk -v2c -c {COMMUNITY} {MODEM_IP} .1.3.6.1.2.1.2.2.1.2 > {OUTPUT_FILE}

REBOOT_MODEM: |
  snmpset -v2c -c {COMMUNITY} {MODEM_IP} .1.3.6.1.2.1.69.1.1.3.0 i 1

# python loader:
import yaml

templates = yaml.safe_load(open("templates/modem_tasks.yaml"))

cmd = templates["REBOOT_MODEM"].format(
    COMMUNITY="public",
    MODEM_IP="192.168.0.2"
)

print(cmd)
```

## list, tuple, set
```
            |   List  |  Tuple |   Set     
------------|---------|--------|--------
Syntax      |   [ ]   |   ( )  |   { }                
------------|---------|--------|--------
Duplicates? |   Yes   |   Yes  |   No             
------------|---------|--------|--------
Ordered?    |   Yes   |   Yes  |   No          
------------|---------|--------|--------
Mutable?    |   Yes   |   No   |   Yes      
------------|---------|--------|--------
```

## python -m
```bash
# how 'python -m module_name' works
Python 解释器 ->   在 sys.path 中查找模块  -->  执行模块的 __main__ 代码  -->  模块作为脚本运行

# list all available modules
python3 -m pydoc modules

# simple http server
python3 -m http.server 8000

# 检查模块路径
python3 -c "import sys; print(sys.path)"

# 添加模块路径
export PYTHONPATH=/path/to/modules

# ​​项目入口
# 使用包结构
python -m myproject

# 依赖管理
# 使用当前环境的pip
python -m pip install -r requirements.txt

# 测试运行
# 执行测试套件
python -m unittest discover

# 代码质量检查
python -m pylint mymodule.py
python -m black mymodule.py
```

## example
```python
#!/usr/bin/env python3
import subprocess
import re
from pathlib import Path

# -------------------------------
# Configuration
# -------------------------------
EVCNAME = "evc-morris-dentist"
SNMPNAME = "snmp-evc-morris-dentist-1"
DHCP_SERVER = "root@10.254.25.42"
DHCP_SERVER_PASSWORD = "vecima@atc"
MIB_DIR = "/home/tcao/mibs"
MIBTABLE_CMTSCMPTR = "DOCS-IF-MIB:docsIfCmtsCmPtr"
MIBTABLE_CMTSCMSTATUS = "DOCS-IF-MIB:docsIfCmtsCmStatusTable"

TMP_MODEM_LIST = Path("/tmp/modem.list")
TMP_MODEM_MAC_IP_LIST = Path("/tmp/modem_mac_ip.list")


# -------------------------------
# Helpers
# -------------------------------
def run_cmd(cmd, capture=True):
    print(f"[RUN] {cmd}")
    result = subprocess.run(cmd, shell=True, text=True, capture_output=capture)
    if capture:
        return result.stdout.strip()
    return None


def hexmac2decmac(hexstr, isep=":", osep="."):
    parts = hexstr.split(isep)
    try:
        return osep.join(str(int(p, 16)) for p in parts)
    except ValueError:
        return ""


def getmodemip(line):
    match = re.search(r"\[([^\]]*)\]", line)
    ipv4, ipv6 = "", ""
    if match:
        for ip in match.group(1).split():
            if "." in ip:
                ipv4 = ip
            elif ":" in ip:
                ipv6 = ip
    if not ipv4:
        ipv4 = "0.0.0.0"
    return f"{ipv4} {ipv6}".strip()


# -------------------------------
# Nomad data collection
# -------------------------------
def get_modem_list():
    cmd = f"""nomad alloc exec -task evc -job {EVCNAME} sh -c 'ncs_cli -u admin <<EOF
show cable modem brief | t
EOF
' > {TMP_MODEM_LIST}"""
    run_cmd(cmd, capture=False)


def parse_modem_list():
    out_lines = []
    with TMP_MODEM_LIST.open() as fin:
        for line in fin:
            if "operational" in line:
                fields = line.strip().split()
                if not fields:
                    continue
                mac = hexmac2decmac(fields[0])
                ip = getmodemip(line)
                out_lines.append(f"{mac} {ip}")
    TMP_MODEM_MAC_IP_LIST.write_text("\n".join(out_lines))
    return out_lines


def get_snmp_nsiport():
    cmd = (
        f"nomad alloc status $(nomad job allocs {SNMPNAME} "
        "| awk '/snmp/{print $1}') 2>/dev/null | awk '/snmp-nsi-port/{print $3}'"
    )
    return run_cmd(cmd)


# -------------------------------
# Optimized SNMP fetching
# -------------------------------
def fetch_snmp_data(snmp_port):
    """Run snmpwalk for both MIB tables once and return text outputs."""
    base_ssh = (
        f"sshpass -p '{DHCP_SERVER_PASSWORD}' ssh -o StrictHostKeyChecking=no "
        f"-o UserKnownHostsFile=/dev/null {DHCP_SERVER} 2>/dev/null"
    )

    cmd_ptr = f"{base_ssh} snmpwalk -On -v2c -c public {snmp_port} {MIBTABLE_CMTSCMPTR} -M {MIB_DIR}"
    cmd_status = f"{base_ssh} snmpwalk -On -v2c -c public {snmp_port} {MIBTABLE_CMTSCMSTATUS} -M {MIB_DIR}"

    print("[INFO] Fetching SNMP pointer table once...")
    cmts_ptr_data = run_cmd(cmd_ptr)

    print("[INFO] Fetching SNMP status table once...")
    cmts_status_data = run_cmd(cmd_status)

    return cmts_ptr_data.splitlines(), cmts_status_data.splitlines()


def run_snmp_analysis(snmp_port):
    # Step 1: Fetch both SNMP tables once
    cmts_ptr_lines, cmts_status_lines = fetch_snmp_data(snmp_port)

    # Step 2: Build modem_mac -> key mapping
    mac_key_map = {}
    for line in cmts_ptr_lines:
        parts = line.strip().split()
        if len(parts) < 2:
            continue
        oid_val = parts[-1]
        for decmac, *_ in (l.split() for l in TMP_MODEM_MAC_IP_LIST.read_text().splitlines()):
            if decmac in line:
                mac_key_map[decmac] = oid_val

    # Step 3: For each modem, find its IP entries from status table
    for decmac, key in mac_key_map.items():
        mac_hex = ":".join(f"{int(p):02x}" for p in decmac.split("."))
        print(f"\nMAC (dec): {decmac}")
        print(f"MAC (hex): {mac_hex}")

        matches = [l for l in cmts_status_lines if key in l and "IpAddress" in l]
        if matches:
            print("\n".join(matches))
        else:
            print("(No IpAddress found)")

        print("\n--------------------------------\n")


# -------------------------------
# Main flow
# -------------------------------
def main():
    get_modem_list()
    modem_lines = parse_modem_list()
    print(f"[INFO] Parsed {len(modem_lines)} modem entries")

    snmp_port = get_snmp_nsiport()
    print(f"[INFO] SNMP NSI Port: {snmp_port}")

    run_snmp_analysis(snmp_port)


if __name__ == "__main__":
    main()
```

## time
[A Beginner’s Guide to the Python time Module](https://realpython.com/python-time-module/)  
[time — Time access and conversions](https://docs.python.org/3/library/time.html)  
[Python Datetime Tutorial: Manipulate Times, Dates, and Time Spans](https://www.dataquest.io/blog/python-datetime-tutorial/)  
[]()  
[]()  
[]()  


## subprocess
[The subprocess Module: Wrapping Programs With Python](https://realpython.com/python-subprocess/)  
[A Guide to Python Subprocess](https://stackify.com/a-guide-to-python-subprocess/)  
[subprocess — Subprocess management](https://docs.python.org/3/library/subprocess.html)  
[shlex — Simple lexical analysis](https://docs.python.org/3/library/shlex.html)  
[How to Use Python's Subprocess Module](https://earthly.dev/blog/python-subprocess/)  
[python-and-pipes](https://lyceum-allotments.github.io/series/)  
[]()  
```python
# command execution
import subprocess
# Simple command execution
#result = subprocess.run(['ls', '-l'], capture_output=True, text=True)
result = subprocess.run(shlex.split("ls -l"), capture_output=True, text=True)
print(result.stdout)

# With additional parameters
result = subprocess.run(
    ['python', 'script.py'],
    capture_output=True,
    text=True,
    timeout=60,
    check=True
)
print(result.stdout)

# How to Pipe Subprocess Outputs to Inputs of Subprocesses
process1 = subprocess.run(["cat","sample.txt"],capture_output=True,text=True)
process2 = subprocess.run(["grep","-n","Python"],capture_output=True,text=True,input=process1.stdout)
print(process_2.stdout)

# example
import subprocess
import re
import sys
import shlex
import time

def get_nomad_job_id(job_name="vmc-evc-morris-dentist"):
    try:
        cmd = f"nomad job status"
        job_status = subprocess.run(shlex.split(cmd), capture_output=True, text=True, check=True)
        my_job_status = subprocess.run(['grep', 'vmc-evc-morris-dentist'], input=job_status.stdout, text=True, capture_output=True, check=True)
        output = my_job_status.stdout.strip()
        match = re.search(rf"^({job_name}[^\s]+)", output)
        if match:
            return match.group(1)
    except subprocess.CalledProcessError as e:
      raise RuntimeError(f"Failed to get Nomad job ID: {e.stderr.strip()}") from e
    except Exception as e:
      raise RuntimeError(f"An error occurred while getting Nomad job ID: {str(e)}") from e

def execute_confd_command(job_id, confd_cmd, filter):
    try:
        nomad_cmd = (
          "nomad", "alloc", "exec",
          "-task", "vmc",
          "-job", job_id,
          "sh", "-c", confd_cmd
        )

        result = subprocess.run(
            nomad_cmd,
            capture_output=True,
            text=True,
            check=True
        )

        #return result.stdout.strip()
        return [line for line in result.stdout.splitlines() if re.search(filter, line)]

    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to execute confd command: {e.stderr.strip()}") from e
        sys.exit(1)

def main():
  try:
      job_id = get_nomad_job_id()
      print(f"Nomad Job ID: {job_id}")

      while True:
          confd_cmd = (
            "confd_cli -u admin <<EOF\n"
            "show configuration DOCS-IF3-MIB docsIf3BondingGrpCfgTable | tab\n"
            "EOF"
          )
          mib_output = execute_confd_command(job_id, confd_cmd, 'downstream')

          confd_cmd = (
            "confd_cli -u admin <<EOF\n"
            "show ccap docsis docs-mac-domain mac-domain morris-dentist_md1_0 downstream-dynamic-bonding-group | tab\n"
            "EOF"
          )
          cli_output = execute_confd_command(job_id, confd_cmd, 'dsdbg')

          if len(mib_output) == len(cli_output):
              print("MIB output:")
              print('\n'.join(mib_output))
              print("CLI output:")
              print('\n'.join(cli_output))
          # Wait for 5 seconds before the next iteration
          time.sleep(5)
      return 0
  except Exception as e:
      print(f"Error: {e}", file=sys.stderr)
      return 1

if __name__ == "__main__":
    sys.exit(main())
```

## re
[Regular Expressions: Regexes in Python (Part 1)](https://realpython.com/regex-python/)  
[Regular Expressions: Regexes in Python (Part 2)](https://realpython.com/regex-python-part-2/)  
[re — Regular expression operations](https://docs.python.org/3/library/re.html)  
[pythex](https://pythex.org/)  
[Regular Expression HOWTO](https://docs.python.org/3/howto/regex.html)  
[]()  
[]()  
[]()  
```python
import re

prefix = "foo"
string = "foo123x bar fle"

regex = rf"^({prefix}[^\s]+)"
re_obj = re.compile(regex)

match = re_obj.search(string)
if match:
    print(match.group(1))

```


## generator
[Python 3: Using "yield from" in Generators - Part 1](http://simeonvisser.com/posts/python-3-using-yield-from-in-generators-part-1.html)  
[Python 3: Using "yield from" in Generators - Part 2](http://simeonvisser.com/posts/python-3-using-yield-from-in-generators-part-2.html)  
[Python Generators](https://www.dataquest.io/blog/python-generators-tutorial/)  
[How to Use Python Generators – Explained With Code Examples](https://www.freecodecamp.org/news/how-to-use-python-generators/)  
[]()  


## idomatic python
[Idiomatic Python. Coding the smart way](https://medium.com/the-andela-way/idiomatic-python-coding-the-smart-way-cc560fa5f1d6)  
[Intermediate and Advanced Software Carpentry](https://intermediate-and-advanced-software-carpentry.readthedocs.io/en/latest/index.html)  
[Writing Idiomatic Python](https://github.com/xx299x/ebooks/tree/master/favorite)  
[]()  
[]()  
```python
# Chained comparison operators
if x <= y <= z:
  # do something

# Use the Falsy & Truthy Concepts
(x, y) = (True, 0)
# x is truthy
if x:
  # do something
else:
  # do something else
# y is falsy
if not y:
  # do something
ls = [2, 5]
if ls:
  # do something

# Ternary Operator replacement
a = True
value = 1 if a else 0
print(value)

# Use the ‘in’ keyword
city = 'Nairobi'
found = city in {'Nairobi', 'Kampala', 'Lagos'}

cities = [‘Nairobi’, ‘Kampala’, ‘Lagos’]
for city in cities:
  print(city)

# Use ‘return’ to evaluate expressions, in addition to return values
def check_equal(x, y):
  return x == y

# Multiple assignment
x = y = z = 'foo'

# Formatting Strings
def user_info(user):
  return 'Name: {user.name} Age: {user.age}'.format(user=user)

# List comprehension
ls = [element for element in range(10) if not(element % 2)]

# enumerate(list)
ls = list(range(10))
for index, value in enumerate(ls):
  print(value, index)

# Dictionary Comprehension
emails = {user.name: user.email for user in users if user.email}

# Sets
ls1 = [1, 2, 3, 4, 5]
ls2 = [4, 5, 6, 7, 8]
elements_in_both = list( set(ls1) & set(ls2) )
print(elements_in_both)

# Set Comprehension
elements = [1, 3, 5, 2, 3, 7, 9, 2, 7]
unique_elements = {element for element in elements}
print(unique_elements)

# Use the default parameter of ‘dict.get’ to provide default values
auth = payload.get('auth_token', 'Unauthorized')

# Don’t Repeat Yourself (DRY)
if user:
  print('{0}\n{1}\n{0}'.format('-'*30, user))
```

## 高级特性
```python
# 切片
l = list(range(100))
l[0:3]     # [0, 1, 2]
l[:3]      # [0, 1, 2]
l[1:3]     # [1, 2]
l[-2:]     # [98, 99] (倒数第一个元素的索引是 -1)
l[-2:-1]   # [98]

l[:10]     # 前 10 个数
l[-10:]    # 后 10 个数

l[:10:2]   # 前 10 个数，每隔两个取一个
l[::5]     # 所有数，每隔5个取一个
l[:]       # 原样复制一个 list

# tuple 的切片操作和 list 一致，操作结果仍是 tuple
# 字符串也可以执行切片操作，结果仍是字符创

## 迭代
iteration 即 给定一个 list 或者 tuple ，通过 for 循环遍历这个 list 或 tuple
Python 的 的迭代通过 for ... in 来完成，只要是可迭代对象，都可以使用 for ... in 进行迭代

for i in list(range(4)):
    print(i)

for i in (1, 2, 3):
    print(i)

for c in 'abc':
    print(c)

d = {'a': 1, 'b': 2, 'c': 3}
for key in d:
    print(key)

# 判断对象是否为可迭代对象
from collections.abc import Iterable
isinstance('abc', Iterable)     # True
isinstance(123, Iterable)       # False

# 利用 enumerate 函数把一个 list 转换为 索引-元素对
for i, v in enumerate(['a', 'b', 'c']):
    print(i, v)

for x, y in [(0, 0), (1, 1), (2, 2)]:
    print(x, y)

## 列表生成式 list comprehension
[x * x for x in range(10)]
[x * x for x in range(10) if x % 2 == 0]
[m + n for m in 'ABC' for n in 'XYZ']

d = {'x': 'A', 'y': 'B', 'z': 'C'}
for k, v in d.items():
    print(k, "=", v)

d = {'x': 'A', 'y': 'B', 'z': 'C'}
[k + "=" + v for k, v in d.items()]

l = ['Hello', 'World', 'IBM', 'Apple']
[s.lower() for s in l]

# 列表生成式与 if ... else
# 在一个列表生成式中，for前面的if ... else是表达式，而for后面的if是过滤条件，不能带else

# 跟在 for 后面的 if 是一个筛选条件，所以不能带 else ，否则如何筛选
[x for x in range(10) if x % 2 == 0]

# 写在 for 前面的 if 是一个表达式，必须根据 x 计算出一个结果，缺少了 else 则无法计算出结果
[x if x % 2 == 0 else -x for x in range(10)]

## 生成器 generator
通过列表生成式，我们可以直接创建一个列表。但是，受到内存限制，列表容量肯定是有限的
生成器 generator 提供了一种推算后续元素的算法，而不是一次性生成所有元素，即提供了一种一边循环一边计算的机制，从而不必创建完整的 list ，从而节省大量的空间

# 创建 generator
l = [x for x in range(10)]  # 列表生成式
g = (x for x in range(10))  # 生成器

next(g)     # 0
next(g)     # 1
next(g)     # 2
...
next(g)     # 9
next(g)     # StopIteration

# generator 是可迭代对象，可以通过 for ... in 进行迭代
for n in (i for i in range(10)):
    print(n)

## generator 函数
函数定义中包含 yield 关键字，该函数就不再是普通函数，而是一个 generator 函数，调用 generator 函数将返回一个 generator
def fib(max):
    n, a, b = 0, 0, 1
    while n < max:
        yield b         # 函数定义中包含 yield 关键字，该函数就不再是普通函数，而是一个 generator 函数，调用 generator 函数将返回一个 generator
        a, b = b, a + b
        n = n + 1
    return 'done'

generator函数 和 普通函数执行流程的差异
普通函数: 顺序执行，遇到 return 语句或者最后一行语句就返回
generator函数: 每次调用 next() 时执行，遇到 yield 关键字 返回，再次执行时从上次返回的 yield 语句处继续执行

def odd():
    print('step 1')
    yield 1
    print('step 2')
    yield 3
    print('step 3')
    yield 5

o = odd()
next(o)     # 1
next(o)     # 3
next(o)     # 5
next(o)     # StopIteration

注意: 调用 generator 函数会创建一个 generator 对象，多次调用 generator 函数会创建多个相互独立的 generator 对象
nex(odd())  # 1
nex(odd())  # 1
nex(odd())  # 1

因此，正确的写法是创建一个 generator 对象，之后不断对这个 generator 对象调用 next()
g = odd()
next(g)
next(g)
next(g)

对于 generator 函数，基本上不会用 next() 来获取下一个返回值，而是直接使用 for 循环进行迭代
使用 for 循环调用 generator 时，如果需要拿到 return 语句的返回值，必须捕获 StopIteration 错误，返回值包含在 StopIteration 的 value 中
g = fib(6)
while True:
    try:
        x = next(g)
        print('g:', x)
    except StopIteration as e:
        print('Generator return value:', e.value)
        break
g: 1
g: 1
g: 2
g: 3
g: 5
g: 8
Generator return value: done

## 迭代器
支持 for 循环的数据类型
集合数据类型: list, tuple, dict, set, str 等
generator: 生成器，带 yield 的 generator 函数

可以直接作用于 for 循环的对象统称为可迭代对象: Iterable
判断一个对象是否是 Iterable 对象
from collections.abc import Iterable
isinstance([], Iterable)    # True
isinstance({}, Iterable)    # True
isinstance('abc', Iterable) # True
isinstance(123, Iterable)   # False

生成器不但可以作用于for循环，还可以被next()函数不断调用并返回下一个值，直到最后抛出StopIteration错误表示无法继续返回下一个值
可以被 next() 函数调用并不断返回下一个值的对象称为迭代器: Iterator
判断一个对象是否是 Iterator 对象
from collections.abc import Iterator
isinstance((x for x in range(10)), Iterator)    # True
isinstance([], Iterator)    # False
isinstance({}, Iterator)    # False
isinstance('abc', Iterator) # False

生成器都是Iterator对象，但list, dict, str虽然是Iterable，却不是Iterator
把list、dict、str等Iterable变成Iterator可以使用iter()函数
isinstance(iter([]), Iterator)      # True
isinstance(iter('abc'), Iterator)   # True

Python 的 Iterator 对象 表示的是一个数据流，可以被 next() 函数调用并不断返回下一个数据，直到没有数据时抛出 StopIteration 错误
可以把这个数据流看做一个有序序列，但是却不能提前知道序列的长度，只能不断通过 next() 获取下一个数据，所以 Iterator 的计算是惰性的，只有在需要返回下一个数据时才会计算
Iterator 可以表示一个无限大的数据流，例如全体自然数，而使用 list 不可能存储全体自然数
```
## 函数式编程
```python
函数式编程就是一种抽象程度很高的编程范式，纯粹的函数式编程语言编写的函数没有变量，因此，任意一个函数，只要输入是确定的，输出就是确定的，这种纯函数我们称之为没有副作用
Python不是纯函数式编程语言，只对函数式编程提供部分支持
# 高阶函数
函数名其实就是指向函数的变量
abs     # <built-in function abs>
f = abs
f       # <built-in function abs>
f(-1)   # 1

abs = 10    # abs这个变量已经不指向求绝对值函数而是指向一个整数10
abs(-10)    # TypeError: 'int' object is not callable

# 如果一个函数就可以接收另一个函数作为参数，该函数就称为高阶函数，高阶函数的作用就是让函数的参数能够接收别的函数
def myadd(x, y, f):
    return f(x) + f(y)

myadd(1, -1, abs)     # 2

## map
map 接收两个参数，一个是函数，一个是 Iterable，map 将传入的函数一次作用到序列的每个元素，并把结果作为新的 Iterator 返回
def f(x):
    return x * x

g = map(f, [1, 2, 3])
list(g)                 # [1, 4, 9]
# Iterator 是惰性序列，通过 list() 函数生成整个序列

list(map(str, [1, 2, 3]))   # ['1', '2', '3']

## reduce
reduce把一个函数作用在一个序列[x1, x2, x3, ...]上，这个函数必须接收两个参数，reduce把结果继续和序列的下一个元素做累积计算

# 1
from functools import reduce

def add(x, y):
    return x + y

reduce(add, [1, 2, 3 ,4, 5])    # 15

# 2
from functools import reduce

def fn(x, y):
    return x * 10 + y

def char2num(c):
    digits = {'0': 0, '1': 1, '2': 2, '3': 3, '4': 4, '5': 5, '6': 6, '7': 7, '8': 8, '9': 9}
    return digits[c]

reduce(fn, map(char2num, '13579'))  # 13579

# 3
from functools import reduce

DIGITS = {'0': 0, '1': 1, '2': 2, '3': 3, '4': 4, '5': 5, '6': 6, '7': 7, '8': 8, '9': 9}

def char2num(c):
    return DIGITS[c]

def str2int(s):
    return reduce(fn, map(char2num, s))

str2int("123")      # 123

## filter
filter() 接收一个函数和一个序列，把传入的函数依次作用于每个元素，然后根据返回值是True还是False决定保留还是丢弃该元素
filter()函数返回的是一个Iterator，也就是一个惰性序列

def not_empty(s):
    return s and s.strip()

list(filter(not_empty, ['A', '', 'B', 'C', '  ']))      # ['A', 'B', 'C']

## sorted
sorted() 用于对 list 进行排序
sorted(['bob', 'about', 'Zoo', 'Credit'])   # ['Credit', 'Zoo', 'about', 'bob']
sorted(['bob', 'about', 'Zoo', 'Credit'], key=str.lower)    # ['about', 'bob', 'Credit', 'Zoo']
sorted(['bob', 'about', 'Zoo', 'Credit'], key=str.lower, reverse=True)  # ['Zoo', 'Credit', 'bob', 'about']

## 返回函数
高阶函数除了可以接受函数作为参数外，还可以把函数作为结果值返回，通常用于不需要立即返回计算结果而是在后面的代码中根据需要再计算结果

def lazy_sum(*args):
    def sum():
        ax = 0
        for n in args:          # 闭包：返回的函数在其定义内部引用了局部变量 args
            ax = ax + n
        return ax
    return sum

f = lazy_sum(1, 3, 5, 7, 9)
f   # <function lazy_sum.<locals>.sum at 0x7f12daae8700>
f() # 25

注意：调用lazy_sum()时，每次调用都会返回一个新的函数，即使传入相同的参数
f1 = lazy_sum(1, 3, 5, 7, 9)
f2 = lazy_sum(1, 3, 5, 7, 9)
f1 == f2    # False

# 闭包
注意:
1. lazy_sum 返回的函数在其定义内部引用了局部变量 args，所以，当一个函数返回了一个函数后，其内部的局部变量还被新函数引用
2. 返回的函数并没有立刻执行，而是直到调用了f()才执行

def count():
    fs = []
    for i in range(1, 4):
        def f():
            return i * i
        fs.append(f)
    return fs

f1, f2, f3 = count()
f1()    # 9
f2()    # 9
f3()    # 9

返回闭包时牢记一点：返回函数不要引用任何循环变量，或者后续会发生变化的变量

# 2 如果一定要引用循环变量，再创建一个函数，用该函数的参数绑定循环变量当前的值，无论该循环变量后续如何更改，已绑定到函数参数的值不变
def count():
    def f(j):
        def g():
            return j * j
        return g
    fs = []
    for i in range(1, 4):
        fs.append(f(i))
    return fs

f1, f2, f3 = count()
f1()    # 1
f2()    # 4
f3()    # 9

缺点是代码较长，可利用lambda函数缩短代码

## nonlocal
使用闭包，就是内层函数引用了外层函数的局部变量。如果只是读外层变量的值，我们会发现返回的闭包函数调用一切正常
def inc():
    x = 0
    def fn():
        return x + 1
    return fn

f = inc()
print(f())  # 1
print(f())  # 1

如果对外层变量赋值，由于Python解释器会把x当作函数fn()的局部变量，会报错
def inc():
    x = 0
    def fn():
        x = x + 1
        return x
    return fn

f = inc()
print(f())  # UnboundLocalError: local variable 'x' referenced before assignment

# 需要在fn()函数内部加一个nonlocal x的声明，加上这个声明后，解释器把fn()的x看作外层函数的局部变量，它已经被初始化了，可以正确计算x+1
def inc():
    x = 0
    def fn():
        nonlocal x
        x = x + 1
        return x
    return fn

注意：使用闭包时，对外层变量赋值前，需要先使用nonlocal声明该变量不是当前函数的局部变量

## 匿名函数
list(map(lambda x: x * x, list(range(5))))

匿名函数 lambda x: x * x

可以被看作

def f(x):
    return x * x

关键字lambda表示匿名函数，冒号前面的x表示函数参数
匿名函数有个限制，就是只能有一个表达式，不用写return，返回值就是该表达式的结果
用匿名函数有个好处，因为函数没有名字，不必担心函数名冲突

# 匿名函数也是一个函数对象，也可以把匿名函数赋值给一个变量，再利用变量来调用该函数
f = lambda x: x * x
f(2)    # 4

# 匿名函数可以作为返回值返回
def build(x, y):
    return lambda: x * x + y * y

## 装饰器 decorator
本质上，decorator就是一个返回函数的高阶函数

def log(func):
    def wrapper(*args, **kw):
        print('call %s()' % func.__name__)  # 函数对象有一个 __name__ 属性，用于返回函数的名称
        return func(*args, **kw)
    return wrapper

@log                    # @log 放到 now() 函数的定义处，相当于执行了 now = log(now)，此时 now 变量指向新的函数，即 log() 函数中返回的 wrapper() 函数
def now():
    print('2025-1-1')

now()   # call now()\n2025-1-1

# 如果 decorator 本身需要传入参数，此时需要编写一个返回 decorator 的高级函数
def log(text):
    def decorator(func):
        def wrapper(*args, **kw):
            print('%s %s():' % (text, func.__name__))
            return func(*args, **kw)
        return wrapper
    return decorator

# 相当于 now = log('execute')(now)，即首先执行log('execute')，返回的是decorator函数，再调用返回的函数，参数是now函数，返回值最终是wrapper函数
@log('execute')
def now():
    print('2025-1-1')

now()   # execute now()\n2025-1-1

# 因为返回的那个wrapper()函数名字就是'wrapper'，所以，需要把原始函数的__name__等属性复制到wrapper()函数中，否则，有些依赖函数签名的代码执行就会出错
now.__name__    # 'wrapper'

Python内置的 functools.wraps 用来处理上述问题

import functools

def log(func):
    @functools.wraps(func)
    def wrapper(*args, **kw):
        print('call %s():' % func.__name__)
        return func(*args, **kw)
    return wrapper

# 带参数的 decorator
def log(text):
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kw):
            print('%s %s():' % (text, func.__name__))
            return func(*args, **kw)
        return wrapper
    return decorator

@log('execute')
def now():
    print('2025-1-1')

now.__name__    # 'now'

## 偏函数
functools.partial的作用就是，把一个函数的某些参数给固定住（也就是设置默认值），返回一个新的函数
# 1
def int2(x, base=2):
    return int(x, base)

functools.partial 用于创建偏函数，而不需要自己定义类似 int2 这样的函数

import functools
int2 = functools.partial(int, base=2)
int2('1000')    # 8
# 新的int2函数，仅仅是把base参数重新设定默认值为2，但也可以在函数调用时传入其他值
int2('1000', base=10)   # 1000

创建偏函数时，实际上可以接收函数对象、*args和**kw这3个参数

当传入 int2 = functools.partial(int, base=2)
实际上固定了 int() 函数的关键字参数 base 即
int2('1000')
相当于
kw = {'base': 2}
int('1000', **kw)

当传入 max2 = functools.partial(max, 10)
实际上会把 10 作为 *args 的一部分自动加到左边，即
max2(5, 6, 7)
相当于
args = (10, 5, 6, 7)
max(*args)
```
## 模块
```python
Python中一个 .py 文件就称之为一个模块（Module）
模块主要是为了提高了代码的可维护性
模块可以避免函数名和变量名冲突

mycompany
├─ __init__.py
├─ abc.py
└─ xyz.py
为了避免模块名冲突，Python又引入了按目录来组织模块的方法，称为包（Package）
例如，mycompany 为顶层包名，每一个包目录下面都会有一个__init__.py的文件，这个文件是必须存在的，否则，Python就把这个目录当成普通目录，而不是一个包
__init__.py可以是空文件，也可以有Python代码，因为__init__.py本身就是一个模块，而它的模块名就是mycompany

# 多级层次的包结构
mycompany
 ├─ web
 │  ├─ __init__.py
 │  ├─ utils.py
 │  └─ www.py
 ├─ __init__.py
 ├─ abc.py
 └─ utils.py
文件www.py的模块名就是mycompany.web.www
两个文件utils.py的模块名分别是mycompany.utils和mycompany.web.utils
mycompany.web也是一个模块，该模块对应的 .py 文件为 mycompany/web/__init__.py

注意：自己创建模块时要注意命名，不能和Python自带的模块名称冲突。例如，系统自带了sys模块，自己的模块就不可命名为sys.py，否则将无法导入系统自带的sys模块

## 导入模块
import sys
导入sys模块后，我们就有了变量sys指向该模块，利用sys这个变量，就可以访问sys模块的所有功能

if __name__ == '__main__':
    test()

当在命令行运行hello模块文件时，Python解释器把一个特殊变量 __name__ 置为 __main__ ，而如果在其他地方导入该hello模块时，if判断将失败
这种if测试可以让一个模块通过命令行运行时执行一些额外的代码，最常见的就是运行测试

## 作用域
有的函数和变量希望仅仅在模块内部使用，这在Python中，是通过 _ 前缀来实现的
公开变量: abc, x1, PI
特殊变量：__xxx__ 例如，__name__, __author__, __doc__ ，这些变量可以被直接引用
非公开变量：_x, __x ，例如 _abc, __abc, private函数和变量“不应该”被直接引用，而不是“不能”被直接引用

private函数或变量用来隐藏模块的内部逻辑，是一种非常有用的代码封装和抽象的方法，即：外部不需要引用的函数全部定义成private，只有外部需要引用的函数才定义为public
def _private_1(name):
    return 'Hello, %s' % name

def _private_2(name):
    return 'Hi, %s' % name

def greeting(name):
    if len(name) > 3:
        return _private_1(name)
    else:
        return _private_2(name)

## 安装第三方模块
pip install xxx

## 模块搜索路径
# 1
import sys
print(sys.path)
sys.path.append('/Users/michael/my_py_scripts')

# 2
设置环境变量PYTHONPATH，该环境变量的内容会被自动添加到模块搜索路径中
```

## 面向对象编程
```python
面向过程的程序设计把计算机程序视为一系列的命令集合，即一组函数的顺序执行。为了简化程序设计，面向过程把函数继续切分为子函数，即把大块函数通过切割成小块函数来降低系统的复杂度

面向对象编程是一种编程思想，把对象作为程序的基本单元，一个对象包含了数据和操作数据的函数
面向对象的程序设计把计算机程序视为一组对象的集合，每个对象都可以接收其他对象发过来的消息，并处理这些消息，计算机程序的执行是一系列消息在各个对象之间传递

Python中所有数据类型都可以视为对象，自定义对象的数据类型即类 Class

# 自定义数据类型
def Student(object):
    def __init__(self, name, score):    # 特殊方法__init__前后分别有两个下划线，self 指向创建的实例本身
        self.name = name                # 数据封装: 每个实例就拥有各自的name和score
        self.score = score
    
    def print_score(self):              # 类的方法，第一个参数永远是实例变量self
        print('%s: %s' % (self.name, self.score))

    def get_grade(self):
        if self.score >= 90:
            return 'A'
        elif self.score >= 60:
            return 'B'
        else:
            return 'C'

给对象发消息实际上就是调用对象对应的关联函数，我们称之为对象的方法（Method）

bart = Student('Bart Simpson', 59)
lisa = Student('Lisa Simpson', 87)
bart.print_score()
lisa.print_score()

面向对象的设计思想是抽象出Class，根据Class创建Instance
面向对象的抽象程度又比函数要高，因为一个Class既包含数据，又包含操作数据的方法

# 类和实例
bart = Student('Bart', 10)  # bart 指向一个 Student 的实例
bart    # <t1.Student object at 0x7fe6dab3a800>

# 访问限制
def Student(object):
    def __init__(self, name, score):
        self.__name = name              # 实例变量名以 __ 开头，是私有变量，只在类的内部可以访问
        self.__score = score

    def get_score(self):
        return self.__score

    def set_score(self, score):         # 在方法中，可以对参数做检查，避免传入无效的参数
        self.__score = score

类似 __xxx__ 的变量名是 特殊变量，特殊变量可以直接访问，特殊变量不是 private 变量

# 继承和多态
class Animal(object):
    def run(self):
        print('Animal is running ...')

class Dog(Animal):
    def run(self):
        print('Dog is running ...')

class Cat(Animal):
    def run(self):
        print('Cat is running ...')

def run_twice(animal):
    animal.run()
    animal.run()

dog = Dog()
cat = Cat()
isinstance(dog, Animal)     # True
isinstance(cat, Animal)     # True

run_twice(dog)      # Dog is running ...
run_twice(cat)      # Cat is running ...

多态的好处：新增一个 Animal 的子类，不必对 run_twice() 做任何修改，任何依赖 Animal 作为参数的函数或者方法可以不加修改地正常运行

def Tortoise(Animal):
    def run(self):
        print('Tortoise is running ...')

run_twice(Tortoise())   # Tortoise is running

"开闭原则"
对扩展开发：允许新增 Animal 子类
对修改封闭：不需要修改依赖 Animal 类型的 run_twice() 等函数

动态语言的 "鸭子类型"：不要求严格的继承体系，一个对象只要“看起来像鸭子，走起路来像鸭子”，那它就可以被看做是鸭子

对于Python这样的动态语言来说，则不一定需要传入Animal类型。我们只需要保证传入的对象有一个run()方法就可以了
class Timer(object):
    def run(self):
        print('Start ...')

t = Timer()
run_twice(t)    # Start ...

# 获取对象信息
# 1 判断对象类型
type(123)   # <class 'int'>
type(abc)   # <class 'builtin_function_or_method'>

type() 函数返回的类型是什么？返回的是对应的 Class 类型

判断一个对象是否是函数
def fn():
    pass

type(fn) == types.FunctionType  # True
type(abs) == types.BuiltinFunctionType  # True
type(abs) == types.BuiltinFunctionType  # True
type((x for x in range(10))) == types.GeneratorType # True

# 使用 instance
对于class的继承关系来说，使用type()就很不方便。我们要判断class的类型，可以使用isinstance()函数
对于继承关系：
object -> Animal -> Dog -> Husky

a = Animal()
d = Dog()
h = Husky()

isinstance(h, Husky)    # True
isinstance(h, Dog)      # True
isinstance(h, Animal)   # True

isinstance('a', str)    # True
isinstance(b'a', bytes) # True

isinstance([1, 2, 3], (list, tuple))    # True
isinstance((1, 2, 3), (list, tuple))    # True

提示：总是优先使用isinstance()判断类型，可以将指定类型及其子类“一网打尽”

# dir()
获取对象的所有属性和方法
dir('ABC')
['__add__', '__class__',..., '__subclasshook__', 'capitalize', 'casefold',..., 'zfill']

类似__xxx__的属性和方法在Python中都是有特殊用途的，比如__len__方法返回长度
调用len()函数试图获取一个对象的长度，实际上，在len()函数内部，它自动去调用该对象的__len__()方法
len('ABC')  # 3
等价于
'ABC'__len__()  # 3

对于自定义类，也可以实现 __len__ 方法
class MyDog(object):
    def __len__(self):
        return 100

dog = MyDog()
len(dog)    # 100

# getattr(), setattr(), hasattr()
操作对象的状态
class MyObject(object):
    def __init__(self):
        self.x = 9
    def power(self):
        return self.x * self.x

obj = MyObject()
hasattr(obj, 'x')   # True
hasattr(obj, 'y')   # False
setattr(obj, 'y', 10)
hasattr(obj, 'y')   # True
getattr(obj, 'y')   # 10
obj.y               # 10
getattr(obj, 'z')   # AttributeError: 'MyObject' object has no attribute 'z'
getattr(obj, 'z', 404)  # 404

获取对象的方法
hasattr(obj, 'power')   # True
getattr(obj, 'power')   # <bound method MyObject.power of <t1.MyObject object at 0x7f84c9aaf520>>
fn = getattr(obj, 'power')
fn                      # <bound method MyObject.power of <t1.MyObject object at 0x7f84c9aaf520>>
fn()                    # 81

注意：只有在不知道对象信息的时候，我们才会去获取对象信息
可以写
sum = obj.x + obj.y
就不要写
sum = getattr(obj, 'x') + getattr(obj, 'y')

正确的例子：
def readImage(fp):
    if hasattr(fp, 'read'):
        return readData(fp)
    return None

# 实例属性和类属性
Python 是动态类型语言，根据类创建的实例可以任意绑定属性，给实例绑定属性的方法是通过实例变量，或者通过 self 变量
class Student(object):
    def __init__(self, name):
        self.name = name        # 实例属性 name
# 1
class Student(object):
    name = 'Student'            # 类属性 name

s = Student()
print(s.name)   # Student 因为实例没有 name 属性，所以会打印类的 name 属性
print(Student.name) # Student 打印类的属性
s.name = 'Michael'  # 给实例绑定 name 属性
print(s.name)       # Michael 实例属性的优先级比类属性高，所以会打印实例的 name 属性
print(Student.name) # Student 打印类的属性
del s.name          # 删除实例的 name 属性
print(s.name)       # Student 因为实例没有 name 属性，所以会打印类的 name 属性

注意：千万不要对实例属性和类属性使用相同的名字，因为相同名称的实例属性将屏蔽掉类属性，但是当你删除实例属性后，再使用相同的名称，访问到的将是类属性

正确的写法
class Student(object):
    className = 'Student'            # 类属性 className
```

# 面向对象高级编程
```python
# __slots__

正常情况下，当我们定义了一个class，创建了一个class的实例后，我们可以给该实例绑定任何属性和方法
class Student(object):
    pass

s = Student()
s.name = 'Michael'  # 动态给实例绑定一个属性

def set_age(self, age):
    self.age = age

from types import MethodType
s.set_age = MethodType(set_age, s)  # 给实例绑定一个方法，对另一个实例是不起作用的
s.set_age(25)
s.age           # 25

# 通过给 类 绑定方法，实现给所有实例绑定方法
def set_score(self, score):
    self.score = score

Student.set_score = set_score
s1 = Student()
s2 = Student()
s1.set_score(100)
s2.set_score(99)

# __slots__ 用来限制实例的属性，例如，只允许对 Student 实例添加 name 和 age 属性
class Student(object):
    __slots__ = ('name', 'age') # 用 tuple 定义允许绑定的属性名称

注意：__slots__ 定义的属性近对当前类实例起作用，对继承的子类是不起作用的，除非在子类中也定义 __slots__ ，这样子类实例允许定义的属性就是自身的__slots__加上父类的__slots__

# @property

为了因此实例的属性，并且检查传入的参数，可以通过添加 get 和 set 方法
class Student(object):
    def get_score(self):
         return self._score

    def set_score(self, value):
        if not isinstance(value, int):
            raise ValueError('score must be an integer!')
        if value < 0 or value > 100:
            raise ValueError('score must between 0 ~ 100!')
        self._score = value

上述方法略显复杂，没有直接用属性这么直接简单，@property 装饰器用来把一个方法变成属性调用

class Student(object):
    @property           # 把 getter 方法变成属性
    def score(self):
        return self._score
    
    @score.setter       # 把 setter 方法变成属性赋值
    def score(self, value):
        if not isinstance(value, int):
            raise ValueError('score must be an integer!')
        if value < 0 or value > 100:
            raise ValueError('score must between 0 ~ 100!')
        self._score = score

s = Student()
s.score = 60        # 实际调用 s.core(60)
s.score             # 实际调用 s.score()
s.score = 999       # ValueError: score must between 0 ~ 100

# 定义只读属性：只定义getter方法，不定义setter方法就是一个只读属性
class Student(object):
    @property
    def birth(self):
        return self._birth

    @birth.setter
    def birth(self, value):
        self._birth = value

    @property               # 只定义getter方法，没有定义setter方法，因此是一个只读属性(因为age可以根据birth和当前时间计算出来)
    def age(self):
        return 2015 - self._birth

注意：属性的方法名不要和实例变量重名，如下为错误代码
class Student(object):
    # 方法名称和实例变量均为birth:
    @property
    def birth(self):
        return self.birth
调用s.birth时，首先转换为方法调用，在执行return self.birth时，又视为访问self的属性，于是又转换为方法调用self.birth()，造成无限递归，最终导致栈溢出报错RecursionError

# 多重继承
通过多重继承，一个子类就可以同时获得多个父类的所有功能

class Animal(object):
    pass

# 大类:
class Mammal(Animal):
    pass

class Bird(Animal):
    pass

# 各种动物:
class Dog(Mammal):
    pass

class Bat(Mammal):
    pass

class Parrot(Bird):
    pass

class Ostrich(Bird):
    pass

要给动物再加上Runnable和Flyable的功能，只需要先定义好Runnable和Flyable的类
class Runnable(object):
    def run(self):
        print('Running...')

class Flyable(object):
    def fly(self):
        print('Flying...')

对于需要Runnable功能的动物，就多继承一个Runnable
class Dog(Mammal, Runnable):
    pass

对于需要Flyable功能的动物，就多继承一个Flyable
class Bat(Mammal, Flyable):
    pass

# MixIn
在设计类的继承关系时，通常，主线都是单一继承下来的，例如，Ostrich继承自Bird
但是，如果需要“混入”额外的功能，通过多重继承就可以实现，比如，让Ostrich除了继承自Bird外，再同时继承Runnable。这种设计通常称之为MixIn

为了更好地看出继承关系，我们把Runnable和Flyable改为RunnableMixIn和FlyableMixIn。类似的，你还可以定义出肉食动物CarnivorousMixIn和植食动物HerbivoresMixIn，让某个动物同时拥有好几个MixIn：
class Dog(Mammal, RunnableMixIn, CarnivorousMixIn):
    pass

# 定制类
形如__xxx__的变量或者函数名，有特殊的用途，可以帮助我们定制类

# __str__
class Student(object):
    def __init__(self, name):
        self.name = name

print(Student('Michael'))   # <__main__.Student object at 0x109afb190>

# 
class Student(object):
    def __init__(self, name):
        self.name = name
    def __str__(self):
        return 'Student object (name: %s)' % self.name

print(Student('Michael'))   # Student object (name: Michael)

s = Student('Michael')
s                           # <__main__.Student object at 0x109afb190>
# 直接显示变量调用的不是__str__()，而是__repr__()，两者的区别是__str__()返回用户看到的字符串，而__repr__()返回程序开发者看到的字符串，即 __repr__()是为调试服务的
# 
class Student(object):
    def __init__(self, name):
        self.name = name
    def __str__(self):
        return 'Student object (name: %s)' % self.name
    __repr__ = __str__

# __iter__
__iter__() 方法返回一个迭代对象，Python的for循环就会不断调用该迭代对象的__next__()方法拿到循环的下一个值，直到遇到StopIteration错误时退出循环

class Fib(object):
    def __init__(self):
        self.a, self.b = 0, 1 # 初始化两个计数器a，b

    def __iter__(self):
        return self # 实例本身就是迭代对象，故返回自己

    def __next__(self):
        self.a, self.b = self.b, self.a + self.b # 计算下一个值
        if self.a > 100000: # 退出循环的条件
            raise StopIteration()
        return self.a # 返回下一个值

for n in Fib():
    print(n)

# __getitem__
Fib实例虽然能作用于for循环，看起来和list有点像，但是，把它当成list来使用还是不行
Fib()[5]    # TypeError: 'Fib' object does not support indexing

要表现得像list那样按照下标取出元素，需要实现__getitem__()方法
class Fib(object):
    def __getitem__(self, n):
        a, b = 1, 1
        for x in range(n):
            a, b = b, a + b
        return a

f = Fib()
f[0]    # 1
f[1]    # 1
f[2]    # 2

list有个神奇的切片方法 
list(range(100))[5:10]  # [5, 6, 7, 8, 9]
对于Fib却报错。原因是__getitem__()传入的参数可能是一个int，也可能是一个切片对象slice，所以要做判断

class Fib(object):
    def __getitem__(self, n):
        if isinstance(n, int): # n是索引
            a, b = 1, 1
            for x in range(n):
                a, b = b, a + b
            return a
        if isinstance(n, slice): # n是切片
            start = n.start
            stop = n.stop
            if start is None:
                start = 0
            a, b = 1, 1
            L = []
            for x in range(stop):
                if x >= start:
                    L.append(a)
                a, b = b, a + b
            return L

f = Fib()
f[0:5]      # [1, 1, 2, 3, 5]
f[:10]      # [1, 1, 2, 3, 5, 8, 13, 21, 34, 55]

以上没有对step参数作处理，也没有对负数作处理，所以，要正确实现一个__getitem__()还是有很多工作要做的

# __setitem__ , __delitem__

通过上面的方法，我们自己定义的类表现得和Python自带的list、tuple、dict没什么区别，这完全归功于动态语言的“鸭子类型”，不需要强制继承某个接口

# _getattr__
正常情况下，调用不存在的类的方法或者属性时，会报错
class Student(object):
    def __inti__(self):
        self.name = 'Michael'

s = Student()
print(s.name)   # Michael
print(s.score)  # AttributeError: 'Student' object has no attribute 'score'

__getattr__() 方法用来动态的返回一个属性，从而避免上述错误

class Student(object):
    def __inti__(self):
        self.name = 'Michael'
    
    def __getattr__(self, attr):
        if attr == 'score':
            return 99

print(s.name)   # Michael
print(s.score)  # 99
# 当调用不存在的属性时，Python 解释器会试图调用 __getattr__ 来尝试获得属性

__getattr__() 方法也可以返回函数

class Student(object):
    def __getattr__(self, attr):
        if attr == 'age':
            return labmda: 25

s = Student()
s.age()     # 25 

注意：只有在没有找到属性的情况下，才会调用 __getattr__

# 指定 Class 只响应特定的几个属性
class Student(object):
    def __getattr__(self, attr):
        if attr == 'age':
            return lambda: 25
        raise AttributeError('\'Student\' object has no attribute \'%s\'' % attr)

# 动态调用的特性的应用
现在很多网站都搞REST API，比如新浪微博、豆瓣啥的，调用API的URL类似
http://api.server/user/friends
http://api.server/user/timeline/list

如果给每个URL对应的API都写一个方法，很繁琐，并且API一旦改动，代码也需要修改
利用完全动态的__getattr__，我们可以写出一个链式调用：
class Chain(object):
    def __init__(self, path=''):
        self._path = path

    def __getattr__(self, path):
        return Chain('%s/%s' % (self._path, path))

    def __str__(self):
        return self._path;

    __repr__ = __str__

Chain().status.user.timeline.list   # /status/user/timeline/list

# __call__

通常，实例方法可以通过 instance.method() 来调用，__call__ 用来直接在实例本身上调用方法

class Student(object):
    def __init__(self, name):
        self._name = name

    def __call__(self):
        print('My name is %s.' % self._name)

s = Student('Michael')
s() # My name is Michael.

对实例进行直接调用就好比对一个函数进行调用一样，所以完全可以把对象看成函数，因为这两者之间没有根本的区别

# 判断一个对象是否能被调用，即是否是一个 Callable 对象
callable(Student())     # True
callable(max)           # True
callable([1, 2, 3])     # False

## 枚举类

# 定义 Month 类型的枚举类
from enum import Enum

Month = Enum('Month', ('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec')) # 每个常量都是 Month 的一个唯一的实例

Month.Jan   # 应用枚举类的常量

# 枚举枚举类的所有成员
for name, memeber in Month.__members__.items():
    print(name, '=>', member, ',', member.value)    # value属性自动赋给成员的int常量，默认从1开始计数

# 从 Enum 派生出自定义类，精确地控制枚举类型
from enum import Enum, unique

@unique                 # @unique装饰器可以帮助我们检查保证没有重复值
class Weekday(Enum):
    Sun = 0
    Mon = 1
    Tue = 2
    Wed = 3
    Thu = 4
    Fri = 5
    Sat = 6

# 访问枚举类型
day1 = Weekday.Mon
print(day1)             # Weekday.Mon
print(Weekday.Tue)      # Weekday.Tue
print(Weekday['Tue'])           # Weekday.Tue
print(Weekday.Tue.value)        # 2
print(day1 == Weekday.Mon)      # True
print(day1 == Weekday.Tue)      # False
print(Weekday(1))               # Weekday.Mon
print(day1 == Weekday(1))       # True
Weekday(7)                      # ValueError: 7 is not a valid Weekday
Weekday(1)                  # <Weekday.Mon: 1>

for name, member in Weekday.__members__.items():
    print(name, '=>', member)

Sun => Weekday.Sun
Mon => Weekday.Mon
Tue => Weekday.Tue
Wed => Weekday.Wed
Thu => Weekday.Thu
Fri => Weekday.Fri
Sat => Weekday.Sat

# 元类
class Hello(object):
    def hello(self, name='world'):
        print('Hello, %s.' % name)

print(type(Hello))  # <class 'type'>，说明 Hello 是一个 class，它的类型是 type

# type() 既可以返回一个对象的类型，也可以创建新的类型

def fn(self, name='world'):
    print('Hello, %s.' % name)

Hello = type('Hello', (object,), dict(hello=fn))    # 创建 Hello class

# metaclass
metaclass 用来控制类的创建行为
定义 metaclass -> 创建类 -> 创建实例

# metaclass 是类的模板，因此必须从 type 类派生
class ListMetaclass(type):
    def __new__(cls, name, bases, attrs):   # 类的对象，类的名字，类继承的父类集合，类的方法集合
        attrs['add'] = lambda self, value: self.append(value)
        return type.__new__(cls, name, bases, attrs)

class MyList(list, metaclass=ListMetaclass):    # 关键字参数 metaclass 指示 Python 解释器在创建 MyList 时，通过 ListMetaclass.__new__ 来创建
    pass

L = MyList()
L.add(1)        # [1]

# 应用实例
ORM全称“Object Relational Mapping”，即对象-关系映射，就是把关系数据库的一行映射为一个对象，也就是一个类对应一个表，这样，写代码更简单，不用直接操作SQL语句
...
```

# 错误处理
```python
try:
    print('try...')
    r = 10 / int('a')
    print('result:', r)
except ValueError as e:
    print('ValueError:', e)
except ZeroDivisionError as e:
    print('ZeroDivisionError:', e)
else:
    print('no error!')
finally:
    print('finally...')
print('END')

Exception hierarchy: https://docs.python.org/3/library/exceptions.html#exception-hierarchy

# try...except 捕获错误的好处是可以跨越多层调用

def foo(s):
    return 10 / int(s)

def bar(s):
    return foo(s) * 2

def main():
    try:
        bar('0')
    except Exception as e:
        print('Error:', e)
    finally:
        print('finally...')
即不需要在每个可能出错的地方去捕获错误，只要在合适的层次去捕获错误就可以了

# 记录错误
logging 模块用来记录错误信息

import logging

def foo(s):
    return 10 / int(s)

def bar(s):
    return foo(s) * 2

def main():
    try:
        bar('0')
    except Exception as e:
        logging.exception(e)

main()
print('END')

程序打印完错误信息后会继续执行，并正常退出

# 抛出错误

# 自定义错误
class FooError(ValueError):
    pass

def foo(s):
    n = int(s)
    if n==0:
        raise FooError('invalid value: %s' % s)
    return 10 / n

foo('0')

尽量使用Python内置的错误类型，只有在必要的时候才定义我们自己的错误类型

# 
def foo(s):
    n = int(s)
    if n==0:
        raise ValueError('invalid value: %s' % s)
    return 10 / n

def bar():
    try:
        foo('0')
    except ValueError as e:
        print('ValueError!')
        raise                   # raise语句如果不带参数，就会把当前错误原样抛出

bar()


# 在except中raise一个Error，还可以把一种类型的错误转化成另一种类型
try:
    10 / 0
except ZeroDivisionError:
    raise ValueError('input error!')

# 断言
def foo(s):
    n = int(s)
    assert n != 0, 'n is zero!'
    return 10 / n

def main():
    foo('0')

启动Python解释器时可以用-O参数来关闭assert，即 python -O err.py ，关闭后，可以把所有的 assert 语句当成 pass 语句

# logging
logging 不仅会抛出错误，而且可以输出到文件

import logging
logging.basicConfig(level=logging.INFO)

s = '0'
n = int(s)
logging.info('n = %d' % n)
print(10 / n)

logging的好处是可以指定记录信息的级别，logging的另一个好处是通过简单的配置，一条语句可以同时输出到不同的地方，比如console和文件

# pdb
python -m pdb err.py

pdb.set_trace()

# 单元测试

# mydict.py
class Dict(dict):
    def __init__(self, **kw):
        super()__init__(**kw)

    def __getattr__(self, key):
        try:
            return self[key]
        except KeyError:
            raise AttributeError(r"'Dict' object has no attribute '%s'" % key)

    def __setattr__(self, key, value):
        self[key] = value

# 单元测试
import unittest

from mydict import Dict

class TestDict(unittest.TestCase):      # 测试类从unittest.TestCase继承
    def test_init(self):                # 以test开头的方法就是测试方法
        d = Dict(a=1, b='test')
        self.assertEqual(d.a, 1)
        self.assertEqual(d.b, 'test')
        self.assertTrue(isinstance(d, dict))

    def test_key(self):
        d = Dict()
        d['key'] = 'value'
        self.assertEqual(d.key, 'value')

    def test_attr(self):
        d = Dict()
        d.key = 'value'
        self.assertTrue('key' in d)
        self.assertEqual(d['key'], 'value')

    def test_keyerror(self):
        d = Dict()
        with self.assertRaises(KeyError):
            value = d['empty']

    def test_attrerror(self):
        d = Dict()
        with self.assertRaises(AttributeError):
            value = d.empty

if __name__ == '__main__':
    unittest.main()

# 运行单元测试

# 1
python mydict_test.py

# 2
python -m unittest mydict_test      # 推荐的做法，因为这样可以一次批量运行很多单元测试，并且，有很多工具可以自动来运行这些单元测试

# setUp 与 setDown
单元测试中两个特殊的setUp()和tearDown()方法。这两个方法会分别在每调用一个测试方法的前后分别被执行

class TestDict(unittest.TestCase):
    def setUp(self):
        print('setUp...')

    def tearDown(self):
        print('tearDown...')

避免了每个测试方法中重复相同的代码

# 文档测试 doctest
Python内置的“文档测试”（doctest）模块可以直接提取注释中的代码并执行测试
doctest严格按照Python交互式命令行的输入和输出来判断测试结果是否正确。只有测试异常的时候，可以用...表示中间一大段烦人的输出
# mydict2.py
class Dict(dict):
    '''
    Simple dict but also support access as x.y style.

    >>> d1 = Dict()
    >>> d1['x'] = 100
    >>> d1.x
    100
    >>> d1.y = 200
    >>> d1['y']
    200
    >>> d2 = Dict(a=1, b=2, c='3')
    >>> d2.c
    '3'
    >>> d2['empty']
    Traceback (most recent call last):
        ...
    KeyError: 'empty'
    >>> d2.empty
    Traceback (most recent call last):
        ...
    AttributeError: 'Dict' object has no attribute 'empty'
    '''
    def __init__(self, **kw):
        super(Dict, self).__init__(**kw)

    def __getattr__(self, key):
        try:
            return self[key]
        except KeyError:
            raise AttributeError(r"'Dict' object has no attribute '%s'" % key)

    def __setattr__(self, key, value):
        self[key] = value

if __name__=='__main__':
    import doctest
    doctest.testmod()
```

# IO 编程
```python
with语句自动调用close()方法
try:
    f = open('/path/to/file', 'r')
    print(f.read())
finally:
    if f:
        f.close()
简化为
with open('/path/to/file', 'r') as f:
    print(f.read())


f.read(size)    # 每次最多读取size个字节的内容
f.readline()    # 每次读取一行
f.readlines()   # 一次读取所有内容并按行返回list

# file-like Object
# 二进制文件
f = open('/Users/michael/test.jpg', 'rb')
# 字符编码
f = open('/Users/michael/gbk.txt', 'r', encoding='gbk')
# 写文件
with open('/Users/michael/test.txt', 'w') as f:
    f.write('Hello, world!')

# StringIO
StringIO 用于在内存中读写 str

# write
from io import StringIO
f = StringIO()
f.write('hello')
f.write(' ')
f.write('world!')
print(f.getvalue()) # hello world!

# read
from io import StringIO
f = StringIO('Hello!\nHi!\nGoodbye!')
while True:
    s = f.readline()
    if s == '':
        break
    print(s.strip())

# BytesIO
BytesIO用来在内存中读写bytes
# write
from io import BytesIO

f = BytesIO()
f.write('中文'.encode('utf-8')) # 写入的不是str，而是经过UTF-8编码的bytes
print(f.getvalue())             # b'\xe4\xb8\xad\xe6\x96\x87'

# read
from io import BytesIO
f = BytesIO(b'\xe4\xb8\xad\xe6\x96\x87')
f.read()                                    # b'\xe4\xb8\xad\xe6\x96\x87'

# 操作文件和目录
import os
os.name     # 'posix'
os.uname()
os.environ

os.path.abspath('.')
os.path.join('/Users/michael', 'testdir')
os.mkdir('/Users/michael/testdir')
os.rmdir('/Users/michael/testdir')
os.path.split('/Users/michael/testdir/file.txt')    # ('/Users/michael/testdir', 'file.txt')
os.path.splitext('/path/to/file.txt')               # ('/path/to/file', '.txt')
os.rename('test.txt', 'test.py')
os.remove('test.py')

copyfile()的函数在shutil模块中，shutil模块可以看做是os模块的补充

[x for x in os.listdir('.') if os.path.isdir(x)]    # 当前目录下的所有目录
[x for x in os.listdir('.') if os.path.isfile(x) and os.path.splitext(x)[1]=='.py']     # 列出所有的.py文件

# 序列化 pickling 与 反序列化 unpickling
pickling: 把变量从内存中变成可存储或传输的过程
unpickling: 把变量内容从序列化的对象重新读到内存里

# pickling
import pickle
d = dict(name='Bob', age=20, score=88)
pickle.dumps(d)
f = open('dump.txt', 'wb')
pickle.dump(d, f)
f.close()

# unpickling
f = open('dump.txt', 'rb')
d = pickle.load(f)
f.close()
d       # {'age': 20, 'score': 88, 'name': 'Bob'}

# json
如果我们要在不同的编程语言之间传递对象，就必须把对象序列化为标准格式，比如XML，但更好的方法是序列化为JSON，JSON表示出来就是一个字符串，可以被所有语言读取，也可以方便地存储到磁盘或者通过网络传输。JSON不仅是标准格式，并且比XML更快，而且可以直接在Web页面中读取，非常方便

JSON表示的对象就是标准的JavaScript语言的对象
JSON 类型       Python 类型
{}              dict
[]              list
"string"        str
1234.56         int/float
true/false      True/False
null            None

# pickling
import json
d = dict(name='Bob', age=20, score=88)
json.dumps(d)

# unpickling
json_str = '{"age": 20, "score": 88, "name": "Bob"}'
json.loads(json_str)    # {'age': 20, 'score': 88, 'name': 'Bob'}
JSON标准规定JSON编码是UTF-8

# JSON 进阶
import json

class Student(object):
    def __init__(self, name, age, score):
        self.name = name
        self.age = age
        self.score = score

s = Student('Bob', 20, 88)
print(json.dumps(s))            # TypeError: <__main__.Student object at 0x10603cc50> is not JSON serializable


可选参数default就是把任意一个对象变成一个可序列为JSON的对象
def student2dict(std):
    return {
        'name': std.name,
        'age': std.age,
        'score': std.score
    }

print(json.dumps(s, default=student2dict))  # {"age": 20, "name": "Bob", "score": 88}

# 把任意class的实例变为dict
print(json.dumps(s, default=lambda obj: obj.__dict__))


def dict2student(d):
    return Student(d['name'], d['age'], d['score'])

json_str = '{"age": 20, "score": 88, "name": "Bob"}'
print(json.loads(json_str, object_hook=dict2student))   # <__main__.Student object at 0x10cd3c190>，打印出的是反序列化的Student实例对象
```

## 多进程
```python
from multiprocessing import Process
import os

def run_proc(name):
    print('Run child process %s (%s)...' % (name, os.getpid()))

if __name == '__main__':
    print('Parent process %s.' % os.getpid())
    p = Process(target=run_proc, args=('test',))
    print('Child process will start.')
    p.start()
    p.join()
    print('Child process end')

# Pool
Pool 用于启动大量子进程
from multiprocessing import Pool
import os, time, random

def long_time_task(name):
    print('Run task %s (%s)...' % (name, os.getpid()))
    start = time.time()
    time.sleep(random.random() * 3)
    end = time.time()
    print('Task %s runs %0.2f seconds.' % (name, (end - start)))

if __name__=='__main__':
    print('Parent process %s.' % os.getpid())
    p = Pool(4)
    for i in range(5):
        p.apply_async(long_time_task, args=(i,))
    print('Waiting for all subprocesses done...')
    p.close()
    p.join()                 # 对Pool对象调用join()方法会等待所有子进程执行完毕，调用join()之前必须先调用close()，调用close()之后就不能继续添加新的Process
    print('All subprocesses done.')

# subprocess
subprocess模块可以让我们非常方便地启动一个外部程序，然后控制其输入和输出

# 1
import subprocess

print('$ nslookup www.python.org')
r = subprocess.call(['nslookup', 'www.python.org'])
print('Exit code:', r)

# 2
import subprocess

print('$ nslookup')
p = subprocess.Popen(['nslookup'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
output, err = p.communicate(b'set q=mx\npython.org\nexit\n')
print(output.decode('utf-8'))
print('Exit code:', p.returncode)
相当于在命令行执行 nslookup ，然后手动输入
set q=mx
python.org
exit

# 进程间通信
multiprocessing模块包装了底层的机制，提供了Queue、Pipes等多种方式来交换数据

from multiprocessing import Process, Queue
import os, time, random

# 写数据进程执行的代码:
def write(q):
    print('Process to write: %s' % os.getpid())
    for value in ['A', 'B', 'C']:
        print('Put %s to queue...' % value)
        q.put(value)
        time.sleep(random.random())

# 读数据进程执行的代码:
def read(q):
    print('Process to read: %s' % os.getpid())
    while True:
        value = q.get(True)
        print('Get %s from queue.' % value)

if __name__=='__main__':
    # 父进程创建Queue，并传给各个子进程：
    q = Queue()
    pw = Process(target=write, args=(q,))
    pr = Process(target=read, args=(q,))
    # 启动子进程pw，写入:
    pw.start()
    # 启动子进程pr，读取:
    pr.start()
    # 等待pw结束:
    pw.join()
    # pr进程里是死循环，无法等待其结束，只能强行终止:
    pr.terminate()

# 多线程
_thread是低级模块，threading是高级模块，对_thread进行了封装

import time, threading

# 新线程执行的代码:
def loop():
    print('thread %s is running...' % threading.current_thread().name)
    n = 0
    while n < 5:
        n = n + 1
        print('thread %s >>> %s' % (threading.current_thread().name, n))
        time.sleep(1)
    print('thread %s ended.' % threading.current_thread().name)

print('thread %s is running...' % threading.current_thread().name)
t = threading.Thread(target=loop, name='LoopThread')
t.start()
t.join()
print('thread %s ended.' % threading.current_thread().name)

# Lock
多进程中，同一个变量，各自有一份拷贝存在于每个进程中，互不影响
多线程中，所有变量都由所有线程共享

balance = 0
lock = threading.Lock()

def run_thread(n):
    for i in range(100000):
        # 先要获取锁:
        lock.acquire()
        try:
            # 放心地改吧:
            change_it(n)
        finally:
            # 改完了一定要释放锁:
            lock.release()

# 多核CPU
import threading, multiprocessing

def loop():
    x = 0
    while True:
        x = x ^ 1

for i in range(multiprocessing.cpu_count()):
    t = threading.Thread(target=loop)
    t.start()

启动与CPU核心数量相同的N个线程，在4核CPU上可以监控到CPU占用率仅有102%，也就是仅使用了一核，Python的线程虽然是真正的线程，但解释器执行代码时，有一个GIL锁：Global Interpreter Lock，任何Python线程执行前，必须先获得GIL锁，然后，每执行100条字节码，解释器就自动释放GIL锁，让别的线程有机会执行。这个GIL全局锁实际上把所有线程的执行代码都给上了锁，所以，多线程在Python中只能交替执行，即使100个线程跑在100核CPU上，也只能用到1个核

Python虽然不能利用多线程实现多核任务，但可以通过多进程实现多核任务。多个Python进程有各自独立的GIL锁，互不影响

# Threadlocal
多线程环境下，每个线程有自己独立的数据，局部变量只有线程本身可见，不会影响别的线程，而全局变量的修改必须加锁
局部变量的问题：函数调用时，参数传递麻烦
def process_student(name):
    std = Student(name)
    # std是局部变量，但是每个函数都要用它，因此必须传进去：
    do_task_1(std)
    do_task_2(std)

def do_task_1(std):
    do_subtask_1(std)
    do_subtask_2(std)

def do_task_2(std):
    do_subtask_2(std)
    do_subtask_2(std)

问题：层层函数调用都需要传递参数，使用全局变量也不行因为每个线程处理不同的对象不能共享

解决思路：用一个全局的 dict 存放所有对象，然后以 thread 自身作为 key 获得线程对应的对象
global_dict = {}

def std_thread(name):
    std = Student(name)
    # 把std放到全局变量global_dict中：
    global_dict[threading.current_thread()] = std
    do_task_1()
    do_task_2()

def do_task_1():
    # 不传入std，而是根据当前线程查找：
    std = global_dict[threading.current_thread()]
    ...

def do_task_2():
    # 任何函数都可以查找出当前线程的std变量：
    std = global_dict[threading.current_thread()]
    ...

优点：消除了std对象在每层函数中的传递问题
缺点：每个函数获取std的代码有点丑

# ThreadLocal 用来解决参数在一个线程中各个函数之间互相传递的问题，ThreadLocal变量虽然是全局变量，但每个线程都只能读写自己线程的独立副本，互不干扰
ThreadLocal最常用的地方就是为每个线程绑定一个数据库连接，HTTP请求，用户身份信息等，这样一个线程的所有调用到的处理函数都可以非常方便地访问这些资源

import threading

# 创建全局 Threadlocal 对象
local_school = threading.local()

def process_student():
    # 获取当前线程关联的 student:
    std = local_school.student
    print('Hello, %s (in %s)' % (std, threading.current_thread().name))

def process_thread(name):
    # 绑定 ThreadLocal 的 student
    local_school.student = name
    process_student()

t1 = threading.Thread(target=process_thread, args=('Alice',), name='Thread-A')
t1 = threading.Thread(target=process_thread, args=('Bob',), name='Thread-B')
t1.start()
t2.start()
t1.join()
t2.join()

Output:
Hello, Alice (in Thread-A)
Hello, Bob (in Thread-B)

# 多进程 vs 多线程
实现多任务通常使用 Master-Worker模式，Master负责分配任务，Worker负责执行任务，因此，多任务环境下，通常是一个Master，多个Worker
如果用多进程实现Master-Worker，主进程就是Master，其他进程就是Worker
如果用多线程实现Master-Worker，主线程就是Master，其他线程就是Worker

多进程模式：
优点：稳定性高，一个子进程崩溃了，不会影响主进程和其他子进程 （当然主进程挂了所有进程就全挂了，但是Master进程只负责分配任务，挂掉的概率低），Apache最早就是采用多进程模式
缺点：创建进程的代价大 (Unix/Linux系统下，用fork调用还行，在Windows下创建进程开销巨大)

多线程模式：
优点：比多进程快一点，但是也快不到哪去
缺点：任何一个线程挂掉都可能直接造成整个进程崩溃，因为所有线程共享进程的内存

# 线程/进程切换
无论是多进程还是多线程，只要数量一多，效率肯定上不去，因此操作系统切换任务本身是需要代价的

# 计算密集型任务 vs IO密集型任务
计算密集型任务：主要消耗CPU资源
IO密集型任务：CPU消耗很少，任务的大部分时间都在等待IO操作完成（IO的速度远远低于CPU和内存的速度），此时不同语言的效率差异几乎可以忽略不计

# 分布式进程
对于 Thread 和 Process 应该优先选用 Process，因为 Process 更稳定，并且 Process 可以分布到多台机器上，而 Thread 只能分布到同一台机器的多个 CPU 上

multiprocessing 模块不但支持多进程，其中managers子模块还支持把多进程分布到多台机器上

#  服务进程负责启动Queue，把Queue注册到网络上，然后往Queue里面写入任务
# task_master.py

import random, time, queue
from multiprocessing.managers import BaseManager

# 发送任务的队列:
task_queue = queue.Queue()
# 接收结果的队列:
result_queue = queue.Queue()

# 从BaseManager继承的QueueManager:
class QueueManager(BaseManager):
    pass

# 把两个Queue都注册到网络上, callable参数关联了Queue对象:
QueueManager.register('get_task_queue', callable=lambda: task_queue)
QueueManager.register('get_result_queue', callable=lambda: result_queue)
# 绑定端口5000, 设置验证码'abc':
manager = QueueManager(address=('', 5000), authkey=b'abc')
# 启动Queue:
manager.start()
# 获得通过网络访问的Queue对象:
task = manager.get_task_queue()
result = manager.get_result_queue()
# 放几个任务进去:
for i in range(10):
    n = random.randint(0, 10000)
    print('Put task %d...' % n)
    task.put(n)
# 从result队列读取结果:
print('Try get results...')
for i in range(10):
    r = result.get(timeout=10)
    print('Result: %s' % r)
# 关闭:
manager.shutdown()
print('master exit.')

# 在另一台机器上启动任务进程（本机上启动也可以）
# task_worker.py

import time, sys, queue
from multiprocessing.managers import BaseManager

# 创建类似的QueueManager:
class QueueManager(BaseManager):
    pass

# 由于这个QueueManager只从网络上获取Queue，所以注册时只提供名字:
QueueManager.register('get_task_queue')
QueueManager.register('get_result_queue')

# 连接到服务器，也就是运行task_master.py的机器:
server_addr = '127.0.0.1'
print('Connect to server %s...' % server_addr)
# 端口和验证码注意保持与task_master.py设置的完全一致:
m = QueueManager(address=(server_addr, 5000), authkey=b'abc')
# 从网络连接:
m.connect()
# 获取Queue的对象:
task = m.get_task_queue()
result = m.get_result_queue()
# 从task队列取任务,并把结果写入result队列:
for i in range(10):
    try:
        n = task.get(timeout=1)
        print('run task %d * %d...' % (n, n))
        r = '%d * %d = %d' % (n, n, n*n)
        time.sleep(1)
        result.put(r)
    except Queue.Empty:
        print('task queue is empty.')
# 处理结束:
print('worker exit.')

Queue之所以能通过网络访问，就是通过QueueManager实现的。由于QueueManager管理的不止一个Queue，所以，要给每个Queue的网络调用接口起个名字，比如get_task_queue
authkey 是为了保证两台机器正常通信，不被其他机器恶意干扰。如果task_worker.py的authkey和task_master.py的authkey不一致，肯定连接不上

[Queue对象的存储](picture)

注意：Queue的作用是用来传递任务和接收结果，每个任务的描述数据量要尽量小。比如发送一个处理日志文件的任务，就不要发送几百兆的日志文件本身，而是发送日志文件存放的完整路径，由Worker进程再去共享的磁盘上读取文件
```
## 正则表达式
```python
# 1
import re

if re.match(r'ABC\-001', 'ABC\-001'):
    print('matched')
else:
    print('not match')

# 切分字符串
re.split(r'[\s\,\;]+', 'a,b;; c  d')    # ['a', 'b', 'c', 'c']

# 提取分组
m = re.match(r'^(\d{3})-(\d{3,8})$', '010-12345')
m.group(0)  # '010-12345'
m.group(1)  # '010'
m.group(2)  # '12345'

# 贪婪匹配 vs 非贪婪匹配

正则匹配默认是贪婪匹配，也就是匹配尽可能多的字符
re.match(r'^(\d+)(0*)$', '102300').groups() # ('102300', '')

通过添加 ? 进行非贪婪匹配
re.match(r'^(\d+?)(0*)$', '102300').groups()    # ('1023', '00')

# 编译正则表达式
re 模块内部使用正则表达式
1.编译正则表达式，如果正则表达式的字符串本身不合法，会报错
2.用编译后的正则表达式去匹配字符串

如果一个正则表达式要重复使用多次，通过预编译正则表达式，可以提高效率
import re

re_telephone = re.compile(r'^(\d{3})-(\d{3,8})$')
re_telephone.match('010-12345').groups()            # ('010', '12345')
```

## 常用内建模块
```python
# datetime
from datetime import datetime

now = datetime.now() # 获取当前datetime

dt = datetime(2015, 4, 19, 12, 20) # 用指定日期时间创建datetime

在计算机中，时间实际上是用数字表示的。1970年1月1日 00:00:00 UTC+00:00时区的时刻称为epoch time，记为0，timestamp即相对于epoch time的秒数
dt = datetime(2015, 4, 19, 12, 20) # 用指定日期时间创建datetime
dt.timestamp() # 把datetime转换为timestamp
注意：Python的 timestamp 是一个浮点数，整数位表示秒

t = 1429417200.0
print(datetime.fromtimestamp(t))    # timestamp转换为datetime (本地时间)
print(datetime.utcfromtimestamp(t)) # UTC时间

cday = datetime.strptime('2015-6-1 18:19:59', '%Y-%m-%d %H:%M:%S')  # str转换为datetime，'%Y-%m-%d %H:%M:%S'规定了日期和时间部分的格式

now = datetime.now()
print(now.strftime('%a, %b %d %H:%M'))  # datetime转换为str

from datetime import datetime, timedelta
now = datetime.now()
now + timedelta(hours=10)
now - timedelta(days=1)
now + timedelta(days=2, hours=12)   # datetime加减

# 本地时间转换为UTC时间
一个datetime类型有一个时区属性tzinfo，但是默认为None，所以无法区分这个datetime到底是哪个时区，除非强行给datetime设置一个时区
from datetime import datetime, timedelta, timezone
tz_utc_8 = timezone(timedelta(hours=8)) # 创建时区UTC+8:00
now = datetime.now()
dt = now.replace(tzinfo=tz_utc_8) # 强制设置为UTC+8:00
如果系统时区恰好是UTC+8:00，那么上述代码就是正确的，否则，不能强制设置为UTC+8:00时区

# 时区转换
utc_dt = datetime.utcnow().replace(tzinfo=timezone.utc) # 拿到UTC时间，并强制设置时区为UTC+0:00

bj_dt = utc_dt.astimezone(timezone(timedelta(hours=8))) # astimezone()将转换时区为北京时间

tokyo_dt = utc_dt.astimezone(timezone(timedelta(hours=9)))  # astimezone()将转换时区为东京时间

tokyo_dt2 = bj_dt.astimezone(timezone(timedelta(hours=9)))  # astimezone()将bj_dt转换时区为东京时间

datetime表示的时间需要时区信息才能确定一个特定的时间，否则只能视为本地时间
如果要存储datetime，最佳方法是将其转换为timestamp再存储，因为timestamp的值与时区完全无关

## collections

# namedtuple
from collections import namedtuple
Point = namedtuple('Point', ['x', 'y'])
p = Point(1, 2)
p.x     # 1
p.y     # 2

Circle = namedtuple('Circle', ['x', 'y', 'r'])  # namedtuple('名称', [属性list])
namedtuple是一个函数，它用来创建一个自定义的tuple对象，并且规定了tuple元素的个数，并可以用属性而不是索引来引用tuple的某个元素
用namedtuple可以很方便地定义一种数据类型，它具备tuple的不变性，又可以根据属性来引用，使用十分方便

# deque
list是线性存储，按索引访问元素很快，数据量大的时候，插入和删除效率很低
deque是为了高效实现插入和删除操作的双向列表，适合用于队列和栈

from collections import deque
q = deque(['a', 'b', 'c'])
q.append('x')
q.appendleft('y')
q                   # deque(['y', 'a', 'b', 'c', 'x'])

# defaultdict
使用dict时，如果引用的Key不存在，就会抛出KeyError。如果希望key不存在时，返回一个默认值，就可以用defaultdict
from collections import defaultdict
dd = defaultdict(lambda: 'N/A')         # 默认值是调用函数返回的，而函数在创建defaultdict对象时传入
dd['key1'] = 'abc'
dd['key1']      # 'abc'
dd['key2']      # 'N/A'

# OrderedDict
OrderedDict的Key会按照插入的顺序排列，不是Key本身排序

od = OrderedDict()
od['z'] = 1
od['y'] = 2
od['x'] = 3
list(od.keys()) # 按照插入的Key的顺序返回 ['z', 'y', 'x']

# OrderedDict可以实现一个FIFO（先进先出）的dict，当容量超出限制时，先删除最早添加的Key
from collections import OrderedDict

class LastUpdatedOrderedDict(OrderedDict):

    def __init__(self, capacity):
        super(LastUpdatedOrderedDict, self).__init__()
        self._capacity = capacity

    def __setitem__(self, key, value):
        containsKey = 1 if key in self else 0
        if len(self) - containsKey >= self._capacity:
            last = self.popitem(last=False)
            print('remove:', last)
        if containsKey:
            del self[key]
            print('set:', (key, value))
        else:
            print('add:', (key, value))
        OrderedDict.__setitem__(self, key, value)

# ChainMap
ChainMap可以把一组dict串起来并组成一个逻辑上的dict。ChainMap本身也是一个dict，但是查找的时候，会按照顺序在内部的dict依次查找

# 应用：ChainMap实现参数的优先级查找，即先查命令行参数，如果没有传入，再查环境变量，如果没有，就使用默认参数
from collections import ChainMap
import os, argparse

# 构造缺省参数:
defaults = {
    'color': 'red',
    'user': 'guest'
}

# 构造命令行参数:
parser = argparse.ArgumentParser()
parser.add_argument('-u', '--user')
parser.add_argument('-c', '--color')
namespace = parser.parse_args()
command_line_args = { k: v for k, v in vars(namespace).items() if v }

# 组合成ChainMap:
combined = ChainMap(command_line_args, os.environ, defaults)

# 打印参数:
print('color=%s' % combined['color'])
print('user=%s' % combined['user'])

# 测试
# 1 没有任何参数时，打印出默认参数
$ python3 use_chainmap.py 
color=red
user=guest
# 2 当传入命令行参数时，优先使用命令行参数
$ python3 use_chainmap.py -u bob
color=red
user=bob
# 3 同时传入命令行参数和环境变量，命令行参数的优先级较高
$ user=admin color=green python3 use_chainmap.py -u bob
color=green
user=bob

# Counter
Counter是一个简单的计数器，例如，统计字符出现的个数
from collections import Counter
c = Counter('programming')
for ch in 'programming':
    c[ch] = c[ch] + 1

c       # Counter({'g': 2, 'm': 2, 'r': 2, 'a': 1, 'i': 1, 'o': 1, 'n': 1, 'p': 1})
c.update('hello') # 也可以一次性update
c       # Counter({'r': 2, 'o': 2, 'g': 2, 'm': 2, 'l': 2, 'p': 1, 'a': 1, 'i': 1, 'n': 1, 'h': 1, 'e': 1})

# argparse
Python内置的sys.argv保存了完整的参数列表
# copy.py
import sys
print(sys.argv)
source = sys.argv[1]
target = sys.argv[2]
# TODO...

对于复杂的参数处理，可以使用内置的argparse库

# 示例：编写一个备份MySQL数据库的命令行程序，需要输入的参数如下
host参数：表示MySQL主机名或IP，不输入则默认为localhost
port参数：表示MySQL的端口号，int类型，不输入则默认为3306
user参数：表示登录MySQL的用户名，必须输入
password参数：表示登录MySQL的口令，必须输入
gz参数：表示是否压缩备份文件，不输入则默认为False
outfile参数：表示备份文件保存在哪，必须输入

# backup.py

import argparse

def main():
    # 定义一个ArgumentParser实例:
    parser = argparse.ArgumentParser(
        prog='backup', # 程序名
        description='Backup MySQL database.', # 描述
        epilog='Copyright(r), 2023' # 说明信息
    )
    # 定义位置参数:
    parser.add_argument('outfile')
    # 定义关键字参数:
    parser.add_argument('--host', default='localhost')
    # 此参数必须为int类型:
    parser.add_argument('--port', default='3306', type=int)
    # 允许用户输入简写的-u:
    parser.add_argument('-u', '--user', required=True)
    parser.add_argument('-p', '--password', required=True)
    parser.add_argument('--database', required=True)
    # gz参数不跟参数值，因此指定action='store_true'，意思是出现-gz表示True:
    parser.add_argument('-gz', '--gzcompress', action='store_true', required=False, help='Compress backup files by gz.')

    # 解析参数:
    args = parser.parse_args()

    # 打印参数:
    print('parsed args:')
    print(f'outfile = {args.outfile}')
    print(f'host = {args.host}')
    print(f'port = {args.port}')
    print(f'user = {args.user}')
    print(f'password = {args.password}')
    print(f'database = {args.database}')
    print(f'gzcompress = {args.gzcompress}')

if __name__ == '__main__':
    main()

# 测试
# 1 输入有效的参数
$ ./backup.py -u root -p hello --database testdb backup.sql
parsed args:
outfile = backup.sql
host = localhost
port = 3306
user = root
password = hello
database = testdb
gzcompress = False

# 2 缺少必要的参数，或者参数不对，将报告详细的错误信息
$ ./backup.py --database testdb backup.sql
usage: backup [-h] [--host HOST] [--port PORT] -u USER -p PASSWORD --database DATABASE outfile
backup: error: the following arguments are required: -u/--user, -p/--password

# 3 如果输入-h，则打印帮助信息
$ ./backup.py -h                          
usage: backup [-h] [--host HOST] [--port PORT] -u USER -p PASSWORD --database DATABASE outfile

Backup MySQL database.

positional arguments:
  outfile

optional arguments:
  -h, --help            show this help message and exit
  --host HOST
  --port PORT
  -u USER, --user USER
  -p PASSWORD, --password PASSWORD
  --database DATABASE
  -gz, --gzcompress     Compress backup files by gz.

Copyright(r), 2023

# base64
Base64是一种用64个字符来表示任意二进制数据的方法，常用于在URL、Cookie、网页中传输少量二进制数据

base64可以直接进行base64的编解码
import base64
base64.b64encode(b'binary\x00string')       # b'YmluYXJ5AHN0cmluZw=='
base64.b64decode(b'YmluYXJ5AHN0cmluZw==')   # b'binary\x00string'

标准的Base64编码后可能出现字符+和/，在URL中就不能直接作为参数，所以又有一种"url safe"的base64编码，其实就是把字符+和/分别变成-和_
base64.b64encode(b'i\xb7\x1d\xfb\xef\xff')              # b'abcd++//'
base64.urlsafe_b64encode(b'i\xb7\x1d\xfb\xef\xff')      # b'abcd--__'
base64.urlsafe_b64decode('abcd--__')                    # b'i\xb7\x1d\xfb\xef\xff'

由于=字符也可能出现在Base64编码中，但=用在URL、Cookie里面会造成歧义，所以，很多Base64编码后会把=去掉
# 标准Base64:
'abcd' -> 'YWJjZA=='
# 自动去掉=:
'abcd' -> 'YWJjZA'
因为Base64是把3个字节变为4个字节，所以，Base64编码的长度永远是4的倍数，因此，需要加上=把Base64字符串的长度变为4的倍数，就可以正常解码了

# struct
struct模块解决bytes和其他二进制数据类型的转换

import struct
# pack函数把任意数据类型变成bytes
struct.pack('>I', 10240099)     # b'\x00\x9c@c'
# unpack把bytes变成相应的数据类型
struct.unpack('>IH', b'\xf0\xf0\xf0\xf0\x80\x80')   # (4042322160, 32896)

# hashlib
哈希算法就是通过哈希函数hash(data)对任意长度的数据data计算出固定长度的哈希digest，目的是为了发现原始数据是否被人篡改过
# 1
import hashlib

md5 = hashlib.md5()
md5.update('how to use md5 in python hashlib?'.encode('utf-8'))
print(md5.hexdigest())

# 如果数据量很大，可以分块多次调用update()
import hashlib

md5 = hashlib.md5()
md5.update('how to use md5 in '.encode('utf-8'))
md5.update('python hashlib?'.encode('utf-8'))
print(md5.hexdigest())

# 
import hashlib

sha1 = hashlib.sha1()
sha1.update('how to use sha1 in '.encode('utf-8'))
sha1.update('python hashlib?'.encode('utf-8'))
print(sha1.hexdigest())

# hmac
Hmac算法：Keyed-Hashing for Message Authentication。它通过一个标准算法，在计算哈希的过程中，把key混入计算过程中

import hmac
message = b'Hello, world!'
key = b'secret'             # key和message都是bytes类型
h = hmac.new(key, message, digestmod='MD5') # 如果消息很长，可以多次调用h.update(msg)
h.hexdigest()                               # 'fa4ee7d173f2d97ee79022d1a7355bcf'

# itertools
itertools提供了非常有用的用于操作迭代对象的函数

itertools提供的几个“无限”迭代器
# count
import itertools
natuals = itertools.count(1)
for n in natuals:
    print(n)

Ouput:
1
2
3
...

# cycle
import itertools
cs = itertools.cycle('ABC') # 注意字符串也是序列的一种
for c in cs:
    print(c)

Ouput:
'A'
'B'
'C'
'A'
'B'
'C'
...

# repeat
ns = itertools.repeat('A', 3)   # 如果提供第二个参数就可以限定重复次数
for n in ns:
    print(n)

Ouput:
A
A
A

# takewhile()等函数根据条件判断来截取出一个有限的序列
natuals = itertools.count(1)
ns = itertools.takewhile(lambda x: x <= 10, natuals)
list(ns)    # [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

# chain
chain()把一组迭代对象串联起来，形成一个更大的迭代器
for c in itertools.chain('ABC', 'XYZ'):
    print(c)
# 迭代效果：'A' 'B' 'C' 'X' 'Y' 'Z'

# groupby
groupby()把迭代器中相邻的重复元素挑出来放在一起
for key, group in itertools.groupby('AAABBBCCAAA'):
    print(key, list(group))

A ['A', 'A', 'A']
B ['B', 'B', 'B']
C ['C', 'C']
A ['A', 'A', 'A']
实际上挑选规则是通过函数完成的，只要作用于函数的两个元素返回的值相等，这两个元素就被认为是在一组的，而函数返回值作为组的key

# 忽略大小写分组
for key, group in itertools.groupby('AaaBBbcCAAa', lambda c: c.upper()):
    print(key, list(group))

A ['A', 'a', 'a']
B ['B', 'B', 'b']
C ['c', 'C']
A ['A', 'A', 'a']

# contextlib
with open('/path/to/file', 'r') as f:
    f.read()


任何对象，只要正确实现了上下文管理，就可以用于with语句

实现上下文管理是通过__enter__和__exit__这两个方法实现的
class Query(object):
    def __init__(self, name):
        self.name = name

    def __enter__(self):
        print('Begin')
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        if exc_type:
            print('Error')
        else:
            print('End')
    
    def query(self):
        print('Query info about %s...' % self.name)


with Query('Bob') as q:
    q.query()

Output:
Begin
Query info about Bob...
End

# contextlib 用于简化 __enter__和__exit__
from contextlib import contextmanager

class Query(object):
    def __init__(self, name):
        self.name = name

    def query(self):
        print('Query info about %s...' % self.name)

@contextmanager
def create_query(name):
    print('Begin')
    q = Query(name)
    yield q
    print('End')

@contextmanager这个decorator接受一个generator，用yield语句把with ... as var把变量输出出去

with create_query('Bob') as q:
    q.query()


# 如果希望在某段代码执行前后自动执行特定代码，可以用@contextmanager实现
@contextmanager
def tag(name):
    print("<%s>" % name)
    yield
    print("</%s>" % name)

with tag("h1"):
    print("hello")
    print("world")

Output:
<h1>
hello
world
</h1>

代码执行顺序：
1.with 语句执行 yield 之前的语句
2.yield 调用执行 with 语句内部的所有语句
3.执行 yield 之后的语句

@contextmanager 通过让用户编写 generator 简化了上下文管理

# @closing
对于没有实现上下文的对象，@closing 的作用就是把任意对象变为上下文对象，并支持 with 语句

from contextlib import closing
from urllib.request import urlopen

with closing(urlopen('https://www.python.org')) as page:
    for line in page:
        print(line)

closing是一个经过@contextmanager装饰的generator

@contextmanager
def closing(thing):
    try:
        yield thing
    finally:
        thing.close()

@contextlib还有一些其他decorator，便于我们编写更简洁的代码

## urllib
urllib 提供了一系列用于操作URL的功能
# Get
# Post
# Handler

## XML

## HTMLParser
HTMLParser 用来方便地解析HTML

## venv
venv为应用提供隔离的Python运行环境，解决了不同应用间安装多版本的冲突问题
```

## 常用第三方模块
```python
# pillow
pillow 提供了操作图像的强大功能，可以通过简单的代码完成复杂的图像处理

# requests
相比于 Python内置的urllib模块，更好的方案是使用第三方库 requests，处理URL资源更为方便

# chardet
chardet 用来检测编码，支持检测中文、日文、韩文等多种语言

# psutil
psutil 用来获取系统信息（CPU, 内存, 磁盘, 网络 ...），主要用来简化运维工作
```
## 图形界面
## 网络编程
```python
# tcp
# tcp_server.py

import socket
import threading
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(('127.0.0.1', 9999))
s.listen(5)

print('Waiting for connection...')

def tcplink(sock, addr):
    print('Accept new connection from %s:%s...' % addr)
    sock.send(b'Welcome!')
    while True:
        data = sock.recv(1024)
        time.sleep(1)
        if not data or data.decode('utf-8') == 'exit':
            break
        sock.send(('Hello, %s!' % data.decode('utf-8')).encode('utf-8'))
    sock.close()
    print('Connection from %s:%s closed.' % addr)

while True:
    sock, addr = s.accept()     #接受一个新连接
    t = threading.Thread(target=tcplink, args=(sock, addr))     # 创建新线程来处理TCP连接
    t.start()

# tcp_client.py

import socket
import threading

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# 建立连接:
s.connect(('127.0.0.1', 9999))
# 接收欢迎消息:
print(s.recv(1024).decode('utf-8'))
for data in [b'Michael', b'Tracy', b'Sarah']:
    # 发送数据:
    s.send(data)
    print(s.recv(1024).decode('utf-8'))
s.send(b'exit')
s.close()

# udp

# udp_server.py
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# 绑定端口:
s.bind(('127.0.0.1', 9999))
print('Bind UDP on 9999...')

while True:
    # 接收数据:
    data, addr = s.recvfrom(1024)
    print('Received from %s:%s.' % addr)
    s.sendto(b'Hello, %s!' % data, addr)

# tcp_client.py
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
for data in [b'Michael', b'Tracy', b'Sarah']:
    # 发送数据:
    s.sendto(data, ('127.0.0.1', 9999))
    # 接收数据:
    print(s.recv(1024).decode('utf-8'))

s.close()
```

## 电子邮件
```python
# 一封电子邮件的旅程：
发件人 -> MUA -> MTA -> MTA -> 若干个MTA -> MDA <- MUA <- 收件人
MUA：Mail User Agent    （通常是 Outlook, Foxmail 等客户端软件）
MTA：Mail Transfer Agent    (Email服务提供商，比如网易、新浪等等)
MDA：Mail Delivery Agent    ()

# SMTP发送邮件
Python对SMTP支持有smtplib和email两个模块，email负责构造邮件，smtplib负责发送邮件

# POP3收取邮件
poplib模块，实现了POP3协议，可以直接用来收邮件，email模块提供的各种类来解析原始文本，变成可阅读的邮件对象
```

## 访问数据库
```python
表是数据库中存放关系数据的集合，一个数据库里面通常都包含多个表，比如学生的表，班级的表，学校的表，等等。表和表之间通过外键关联
要操作关系数据库，首先需要连接到数据库，一个数据库连接称为Connection
连接到数据库后，需要打开游标，称之为Cursor，通过Cursor执行SQL语句，然后，获得执行结果


# SQLite
SQLite是一种嵌入式数据库，它的数据库就是一个文件。由于SQLite本身是C写的，而且体积很小，所以，经常被集成到各种应用程序中，甚至在iOS和Android的App中都可以集成
import sqlite3

conn = sqlite3.connect('test.db')   # 如果文件不存在，会自动在当前目录创建
cursor = conn.cursor()
cursor.execute('create table user (id varchar(20) primary key, name varchar(20))')
cursor.execute('insert into user (id, name) values (\'1\', \'Michael\')')
cursor.rowcount     # 通过rowcount获得插入的行数
conn.commit()       # 提交事务
cursor.close()      # 关闭Cursor
conn.close()        # 关闭Connection

conn = sqlite3.connect('test.db')
cursor = conn.cursor()
cursor.execute('select * from user where id=?', ('1',))     # 执行查询语句
values = cursor.fetchall()                                  # 获得查询结果集
values                                                      # [('1', 'Michael')]
cursor.close()
conn.close()

# MySQL
MySQL是Web世界中使用最广泛的数据库服务器
SQLite的特点是轻量级、可嵌入，但不能承受高并发访问，适合桌面和移动应用。而MySQL是为服务器端设计的数据库，能承受高并发访问，同时占用的内存也远远大于SQLite。
此外，MySQL内部有多种数据库引擎，最常用的引擎是支持数据库事务的InnoDB

pip install mysql-connector-python


# ORM框架 SQLAlchemy
数据库表是一个二维表，包含多行多列。把一个表的内容用Python的数据结构表示出来的话，可以用一个list表示多行，list的每一个元素是tuple，表示一行记录
比如，包含id和name的user表
[
    ('1', 'Michael'),
    ('2', 'Bob'),
    ('3', 'Adam')
]
用tuple表示一行很难看出表的结构。如果把一个tuple用class实例来表示，就可以更容易地看出表的结构
class User(object):
    def __init__(self, id, name):
        self.id = id
        self.name = name

[
    User('1', 'Michael'),
    User('2', 'Bob'),
    User('3', 'Adam')
]
ORM技术：Object-Relational Mapping，把关系数据库的表结构映射到对象上，这种转换是通过 ORM 框架实现的，在Python中，最有名的ORM框架是SQLAlchemy

pip install sqlalchemy

```

## Web开发
```python
Client/Server模式，不适合Web，最大的原因是Web应用程序的修改和升级非常迅速，而CS架构需要每个客户端逐个升级桌面App
Browser/Server模式：客户端只需要浏览器，应用程序的逻辑和数据都存储在服务器端。浏览器只需要请求服务器，获取Web页面，并把Web页面展示给用户即可

# HTTP
HTML是一种用来定义网页的文本，会HTML，就可以编写网页
HTTP是在网络上传输HTML的协议，用于浏览器和服务器的通信

HTML定义了页面的内容，CSS来控制页面元素的样式，而JavaScript负责页面的交互逻辑

# HTML
# hello.html
<html>
<head>
  <title>Hello</title>
</head>
<body>
  <h1>Hello, world!</h1>
</body>
</html>

# CSS(Cascading Style Sheets)
CSS用来控制HTML里的所有元素如何展现，比如，给标题元素<h1>加一个样式，变成48号字体，灰色，带阴影
<html>
<head>
  <title>Hello</title>
  <style>
    h1 {
      color: #333333;
      font-size: 48px;
      text-shadow: 3px 3px 3px #666666;
    }
  </style>
</head>
<body>
  <h1>Hello, world!</h1>
</body>
</html>

# JavaScript
JavaScript是为了让HTML具有交互性而作为脚本语言添加的，JavaScript既可以内嵌到HTML中，也可以从外部链接到HTML中
如果我们希望当用户点击标题时把标题变成红色，就必须通过JavaScript来实现
<html>
<head>
  <title>Hello</title>
  <style>
    h1 {
      color: #333333;
      font-size: 48px;
      text-shadow: 3px 3px 3px #666666;
    }
  </style>
  <script>
    function change() {
      document.getElementsByTagName('h1')[0].style.color = '#ff0000';
    }
  </script>
</head>
<body>
  <h1 onclick="change()">Hello, world!</h1>
</body>
</html>

# WSGI 接口 (Web Server Gateway Interface)
Web应用的本质：
1.浏览器发送一个HTTP请求；
2.服务器收到请求，生成一个HTML文档；
3.服务器把HTML文档作为HTTP响应的Body发送给浏览器；
4.浏览器收到HTTP响应，从HTTP Body取出HTML文档并显示

简单的Web应用是先把HTML用文件保存好，用一个现成的HTTP服务器软件，接收用户请求，从文件中读取HTML，返回。Apache、Nginx、Lighttpd等是常见的静态服务器
如果要动态生成HTML，需要自己实现HTTP请求、解析HTTP请求、发送HTTP响应这些底层代码，正确的做法是底层代码由专门的服务器软件实现，只用Python专注于生成HTML文档。因为我们不希望接触到TCP连接、HTTP原始请求和响应格式，所以，需要一个统一的接口，让我们专心用Python编写Web业务，这个接口就是WSGI：Web Server Gateway Interface

Python内置了一个WSGI服务器，这个模块叫wsgiref，考虑任何运行效率，仅供开发和测试使用

# hello.py
def application(environ, start_response):       # application()函数由WSGI服务器调用
    start_response('200 OK', [('Content-Type', 'text/html')])
    return [b'<h1>Hello, web!</h1>']

# server.py
from wsgiref.simple_server import make_server
from hello import application

httpd = make_server('', 8000, application)
print('Serving HTTP on port 8000...')

httpd.serve_forever()   # 开始监听HTTP请求

# Web框架
WSGI提供的接口比较低级，需要在WSGI接口之上能进一步抽象，让用户专注于用一个函数处理一个URL，至于URL到函数的映射，则交给Web框架来做
Flask 是一个目前比较流行的Web框架
Flask通过Python的装饰器在内部自动地把URL和函数给关联起来
# 
from flask import Flask
from flask import request

app = Flask(__name__)

@app.route('/', methods=['GET', 'POST'])
def home():
    return '<h1>Home</h1>'

@app.route('/signin', methods=['GET'])
def signin_form():
    return '''<form action="/signin" method="post">
              <p><input name="username"></p>
              <p><input name="password" type="password"></p>
              <p><button type="submit">Sign In</button></p>
              </form>'''

@app.route('/signin', methods=['POST'])
def signin():
    # 需要从request对象读取表单内容：
    if request.form['username']=='admin' and request.form['password']=='password':
        return '<h3>Hello, admin!</h3>'
    return '<h3>Bad username or password.</h3>'

if __name__ == '__main__':
    app.run()

常见的Python Web框架：
Flask
Django
web.py
Bottle
Tornado

# 模板
Web App最复杂的部分在HTML页面，HTML不仅要正确，还要通过CSS美化，再加上复杂的JavaScript脚本来实现各种交互和动画效果。总之，生成HTML页面的难度很大
使用模板，则是预先准备一个HTML文档，这个HTML文档不是普通的HTML，而是嵌入了一些变量和指令，然后，根据我们传入的数据，替换后，得到最终的HTML，发送给用户，即MVC：Model-View-Controller

# MVC 模式
from flask import Flask, request, render_template

app = Flask(__name__)

@app.route('/', methods=['GET', 'POST'])
def home():
    return render_template('home.html') # render_template 用来实现模板的渲染

@app.route('/signin', methods=['GET'])
def signin_form():
    return render_template('form.html')

@app.route('/signin', methods=['POST'])
def signin():
    username = request.form['username']
    password = request.form['password']
    if username=='admin' and password=='password':
        return render_template('signin-ok.html', username=username)
    return render_template('form.html', message='Bad username or password', username=username)

if __name__ == '__main__':
    app.run()

Flask默认支持的模板是jinja2 (pip install jinja2)

# jinja2模板

# home.html
<html>
<head>
  <title>Home</title>
</head>
<body>
  <h1 style="font-style:italic">Home</h1>
</body>
</html>

# form.html
<html>
<head>
  <title>Please Sign In</title>
</head>
<body>
  {% if message %}
  <p style="color:red">{{ message }}</p>
  {% endif %}
  <form action="/signin" method="post">
    <legend>Please sign in:</legend>
    <p><input name="username" placeholder="Username" value="{{ username }}"></p>
    <p><input name="password" placeholder="Password" type="password"></p>
    <p><button type="submit">Sign In</button></p>
  </form>
</body>
</html>

# signin-ok.html
<html>
<head>
  <title>Welcome, {{ username }}</title>
</head>
<body>
  <p>Welcome, {{ username }}!</p>
</body>
</html>

一定要把模板放到正确的templates目录下，templates和app.py在同级目录下
app.py
templates
    form.html
    home.html
    signin-ok.html

通过MVC，在Python代码中处理M：Model和C：Controller，而V：View是通过模板处理的，成功地把Python代码和HTML代码最大限度地分离了

常见的模板:
Jinja2
Mako
Cheetah
Django

```
## 异步IO
```python
# 协程 Coroutine
协程执行过程中，在子程序内部可中断，然后转而执行别的子程序，在适当的时候再返回来接着执行
协程最大的优势是执行效率，它没有线程切换的开销，和多线程比，线程数量越多，协程的性能优势就越明显
第二大优势就是不需要多线程的锁机制，因为只有一个线程，也不存在同时写变量冲突，在协程中控制共享资源不加锁，只需要判断状态

Python对协程的支持是通过generator实现

Python的yield不但可以返回一个值，它还可以接收调用者发出的参数

传统的生产者-消费者模型是一个线程写消息，一个线程取消息，通过锁机制控制队列和等待，但一不小心就可能死锁
改用协程，生产者生产消息后，直接通过yield跳转到消费者开始执行，待消费者执行完毕后，切换回生产者继续生产，效率极高

#
def consumer():
    r = ''
    while True:
        n = yield r                                     # consumer通过yield拿到消息，处理，又通过yield把结果传回
        if not n:
            return
        print('[CONSUMER] Consuming %s...' % n)
        r = '200 OK'

def produce(c):
    c.send(None)                                        # 启动生成器
    n = 0
    while n < 5:
        n = n + 1
        print('[PRODUCER] Producing %s...' % n)
        r = c.send(n)                                   # 切换到consumer执行
        print('[PRODUCER] Consumer return: %s' % r)     # producer拿到consumer处理的结果，继续生产下一条消息
    c.close()                                           # 关闭consumer

c = consumer()
produce(c)

整个流程无锁，由一个线程执行，produce和consumer协作完成任务，所以称为“协程”，而非线程的抢占式多任务

# asyncio
asyncio的编程模型就是一个消息循环。asyncio模块内部实现了EventLoop，把需要执行的协程扔到EventLoop中执行，就实现了异步IO
用asyncio提供的@asyncio.coroutine可以把一个generator标记为coroutine类型，然后在coroutine内部用yield from调用另一个coroutine实现异步操作
Python 3.5 引入了新的语法async和await，可以让coroutine的代码更简洁易读

# 
async def hello(name):
    print("Hello %s! (%s)" % (name, threading.current_thread))
    await asyncio.sleep(1)                                              # 主线程并未等待，而是去执行EventLoop中其他可以执行的async函数了，因此可以实现并发执行
    print("Hello %s again! (%s)" % (name, threading.current_thread))
    return name

async def main():
    L = await asyncio.gather(hello("Bob"), hello("Alice"))
    print(L)

asyncio.run(main())

# 用asyncio的异步网络连接来获取sina、sohu和163的网站首页
import asyncio

async def wget(host):
    print(f"wget {host}...")
    # 连接80端口:
    reader, writer = await asyncio.open_connection(host, 80)
    # 发送HTTP请求:
    header = f"GET / HTTP/1.0\r\nHost: {host}\r\n\r\n"
    writer.write(header.encode("utf-8"))
    await writer.drain()

    # 读取HTTP响应:
    while True:
        line = await reader.readline()
        if line == b"\r\n":
            break
        print("%s header > %s" % (host, line.decode("utf-8").rstrip()))
    # Ignore the body, close the socket
    writer.close()
    await writer.wait_closed()
    print(f"Done {host}.")

async def main():
    await asyncio.gather(wget("www.sina.com.cn"), wget("www.sohu.com"), wget("www.163.com"))

asyncio.run(main())

# aiohttp
asyncio实现了TCP、UDP、SSL等协议，aiohttp 则是基于asyncio实现的HTTP框架
使用aiohttp时，定义处理不同URL的async函数，然后通过app.add_routes()添加映射，最后通过run_app()以asyncio的机制启动整个处理流程

# 编写一个HTTP服务器，分别处理以下URL
/ - 首页返回Index Page
/{name} - 根据URL参数返回文本Hello, {name}!

# app.py
from aiohttp import web

async def index(request):
    text = "<h1>Index Page</h1>"
    return web.Response(text=text, content_type="text/html")

async def hello(request):
    name = request.match_info.get("name", "World")
    text = f"<h1>Hello, {name}</h1>"
    return web.Response(text=text, content_type="text/html")

app = web.Application()

# 添加路由:
app.add_routes([web.get("/", index), web.get("/{name}", hello)])

if __name__ == "__main__":
    web.run_app(app)

```
