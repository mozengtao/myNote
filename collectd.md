```bash
# collectd -h
Usage: collectd [OPTIONS]

Available options:
  General:
    -C <file>       Configuration file.
                    Default: /etc/collectd.conf
    -t              Test config and exit.
    -T              Test plugin read and exit.
    -P <file>       PID-file.
                    Default: /var/run/collectd.pid
    -f              Don't fork to the background.
    -B              Don't create the BaseDir
    -h              Display help (this message)

Builtin defaults:
  Config file       /etc/collectd.conf
  PID file          /var/run/collectd.pid
  Plugin directory  /usr/lib/collectd
  Data directory    /var/lib/collectd

collectd 5.12.0.git, http://collectd.org/
by Florian octo Forster <octo@collectd.org>

```
- [Collectd 101](https://wiki.anuket.io/display/HOME/Collectd+101)
- [collectd.conf(5)](https://www.collectd.org/documentation/manpages/collectd.conf.html)
- [types.db(5)](https://www.collectd.org/documentation/manpages/types.db.html)
- [Plugin architecture](https://github.com/collectd/collectd/wiki/Plugin-architecture)