[Learning to love systemd](https://opensource.com/article/20/4/systemd)  

[System and Service Manager](https://systemd.io/)  
[The systemd for Administrators Blog Series](https://systemd.io/)  
[freedesktop](https://www.freedesktop.org/wiki/Software/systemd/)  

[Understanding Systemd Units and Unit Files](https://www.digitalocean.com/community/tutorials/understanding-systemd-units-and-unit-files)  

![understanding-systemd](./assets/understanding-systemd.pdf)  
![systemd-basics](./assets/systemd-basics.pdf)  
![systemd-management](./assets/systemd-management.pdf)  

``` bash
## 基本概念
unit 是systemd可以操作和管理的任何资源
unit file 是用来定义 unit 对应资源的配置文件
unit file 存放路径：
/lib/systemd/system # 系统安装软件时配置文件的默认存放路径
```
- systemd 以 unit 为基本对象对资源进行管理和配置，unit 代表了可被 systemd 管理的一种资源
- systemd 架构
	- ![](https://www.ruanyifeng.com/blogimg/asset/2016/bg2016030703.png)
-
- unit 类型
	- service
	- socket
	- target
	- device
	- mount
	- automount
	- swap
	- timer
	- path
	- slice
	- scope
-
- target
	- target 是一组相关的 unit 集合，用来方便unit的管理
-
- 示例
	- ```sh
	  systemctl get-default
	  systemctl list-units --type=service --state=failed
	  systemctl list-unit-files --type=target
	  systemctl cat docker.service
	  systemctl show httpd.service
	  ```
- [[sysvinit]]
- 创建systemd timer
	- ```bash
		#To run a unit at specified times or intervals you need two units:
			1. a service unit that defines what to run
			2. a timer unit that defines when to run the service unit
		参考(https://askubuntu.com/questions/1083537/how-do-i-properly-install-a-systemd-timer-and-service)

		#1 创建 example.service
		[Unit]
		Description=An example oneshot service that runs a program

		[Service]
		Type=oneshot
		ExecStart=/tmp/test/timer/mytimer.sh

		[Install]
		WantedBy=multi-user.target

		#2 创建 example.timer
		[Unit]
		Description=A timer that runs our example service

		[Timer]
		OnCalendar=*-*-* *:00/3:00

		[Install]
		WantedBy=timers.target

		#3 拷贝 example.service 和 example.timer到系统路径
		sudo cp example.service /lib/systemd/system
		sudo cp example.timer /lib/systemd/system

		#4 相关命令
		systemctl status example.timer
		sudo systemctl start example.timer
		sudo systemctl status example.timer
		systemctl list-timers
		systemctl status example.timer
		systemd-analyze calendar '*-*-* *:00/3:00'  # success
		systemd-analyze calendar '*-* zz*:00/3:00'  # fail
		sudo systemd-analyze verify /lib/systemd/system/logrotate.timer
	```

[archlinux systemd help](https://wiki.archlinux.org/title/Systemd)  
[How do I properly install a systemd timer and service]((https://askubuntu.com/questions/1083537/how-do-i-properly-install-a-systemd-timer-and-service)  
[**systemd Documentation**](https://0pointer.de/blog/projects/systemd-docs.html)  
[**systemd* help doc**](https://www.freedesktop.org/software/systemd/man/latest/)  
![LinuxServiceManagementMadeEasyWithSystemd.pdf](./assets/LinuxServiceManagementMadeEasyWithSystemd.pdf)  
[systemd doc](http://0pointer.de/blog/projects/systemd-docs.html)  
[man systemd](https://man7.org/linux/man-pages/man1/init.1.html)  
[man systemctl](https://man7.org/linux/man-pages/man1/systemctl.1.html)  
[man systemd.unit](https://man7.org/linux/man-pages/man5/systemd.unit.5.html)  
[systemd blog series](https://www.freedesktop.org/wiki/Software/systemd/)  
[man journalctl](https://man7.org/linux/man-pages/man1/journalctl.1.html)  
[The systemd for Administrators Blog Series](https://www.freedesktop.org/wiki/Software/systemd/)  
[**Working with systemd Timers**](https://documentation.suse.com/smart/systems-management/html/systemd-working-with-timers/index.html)  
[Working with systemd Timers pdf](https://documentation.suse.com/smart/systems-management/pdf/systemd-working-with-timers_en.pdf)  
[Systemd timers onCalendar (cron) format explained](https://silentlad.com/systemd-timers-oncalendar-(cron)-format-explained)  
## Systemd Timer OnCalendar Format
```bash
Format: * *-*-* *:*:*
3 parts:
1. *
	signify the day of the week eg:- Sat,Thu,Mon
2. *-*-*
	signify the calendar date. Which means it breaks down to - year-month-date
3. *:*:*
	signify the time component of the calnedar event. So it is - hour:minute:second

Examples:
	Explaination			Systemd timer
	Every Minute			*-*-* *:*:00
	Every 2 minute			*-*-* *:*/2:00
	Every 5 minutes			*-*-* *:*/5:00
	Every 15 minutes		*-*-* *:*/15:00
	Every quarter hour		*-*-* *:*/15:00
	Every 30 minutes		*-*-* *:*/30:00
	Every half an hour		*-*-* *:*/30:00
	Every 60 minutes		*-*-* */1:00:00
	Every 1 hour			*-*-* *:00:00
	Every 2 hour			*-*-* */2:00:00
	Every 3 hour			*-*-* */3:00:00
	Every other hour		*-*-* */2:00:00
	Every 6 hour			*-*-* */6:00:00
	Every 12 hour			*-*-* */12:00:00
	Hour Range				*-*-* 9-17:00:00
	Between certain hours	*-*-* 9-17:00:00
	Every day				*-*-* 00:00:00
	Daily					*-*-* 00:00:00
	Once A day				*-*-* 00:00:00
	Every Night				*-*-* 01:00:00
	Every Day at 1am		*-*-* 01:00:00
	Every day at 2am		*-*-* 02:00:00
	Every morning			*-*-* 07:00:00
	Every midnight			*-*-* 00:00:00
	Every day at midnight		*-*-* 00:00:00
	Every night at midnight		*-*-* 00:00:00
	Every sunday				Sun *-*-* 00:00:00
	Every friday				Fri *-*-* 01:00:00
	Every friday at midnight	Fri *-*-* 00:00:00
	Every saturday				Sat *-*-* 00:00:00
	Every weekday				Mon...Fri *-*-* 00:00:00
	weekdays only				Mon...Fri *-*-* 00:00:00
	monday to friday			Mon...Fri *-*-* 00:00:00
	Every weekend				Sat,Sun *-*-* 00:00:00
	weekends only				Sat,Sun *-*-* 00:00:00
	Every 7 days				* *-*-* 00:00:00
	Every week					Sun *-*-* 00:00:00
	weekly	Sun 				*-*-* 00:00:00
	once a week					Sun *-*-* 00:00:00
	Every month					* *-*-01 00:00:00
	monthly						* *-*-01 00:00:00
	once a month				* *-*-01 00:00:00
	Every quarter				* *-01,04,07,10-01 00:00:00
	Every 6 months				* *-01,07-01 00:00:00
	Every year					* *-01-01 00:00:00
```
[systemd-analyze](https://www.freedesktop.org/software/systemd/man/latest/systemd-analyze.html#)  
[How to use systemd timers (cronjob alternative)](https://silentlad.com/how-to-use-systemd-timers-(cronjob-alternative))  


## what is systemd
systemd is a suite of system management daemons, libraries, and utilities designed as a central management and configuration platform for the Linux computer operating system.

systemd is not just the name of the init daemon but also refers to the entire software bundle around it, which, in addition to the systemd init daemon, includes the daemons journald, logind and networkd, and many other low-level components. 

systemd is a daemon that manages other daemons, which, including systemd itself, are background processes. systemd is the first daemon to start during booting and the last daemon to terminate during shutdown. 

systemd executes elements of its startup sequence in parallel, which is faster than the traditional startup sequence’s sequential approach. 

systemd records initialization instructions for each daemon in a configuration file (referred to as a “unit file”) that uses a declarative language, replacing the traditionally used per-daemon startup shell scripts. Unit file types include service, socket, device, mount, automount, swap, target, path, timer (which can be used as a cron-like job scheduler), snapshot, slice and scope.

## unit
Units are the objects that systemd knows how to manage.

systemd unit advantages over other init systems:
1. socket-based activation: Sockets associated with a service are best broken out of the daemon itself in order to be handled separately. This provides a number of advantages, such as delaying the start of a service until the associated socket is first accessed. This also allows the system to create all sockets early in the boot process, making it possible to boot the associated services in parallel.
2. bus-based activation: Units can also be activated on the bus interface provided by D-Bus. A unit can be started when an associated bus is published.
3. path-based activation: A unit can be started based on activity on or the availability of certain filesystem paths. This utilizes inotify.
4. device-based activation: Units can also be started at the first availability of associated hardware by leveraging udev events.
5. implicit dependency mapping: Most of the dependency tree for units can be built by systemd itself. You can still add dependency and ordering information, but most of the heavy lifting is taken care of for you.
6. instances and templates: Template unit files can be used to create multiple instances of the same general unit. This allows for slight variations or sibling units that all provide the same general function.
7. easy security hardening: Units can implement some fairly good security features by adding simple directives. For example, you can specify no or read-only access to part of the filesystem, limit kernel capabilities, and assign private /tmp and network access.
8. drop-ins and snippets: Units can easily be extended by providing snippets that will override parts of the system’s unit file. This makes it easy to switch between vanilla and customized unit implementations.

### Where are Systemd Unit Files Found?
The system’s copy of unit files are generally kept in the /lib/systemd/system directory. When software installs unit files on the system, this is the location where they are placed by default

If you need to modify the system’s copy of a unit file, putting a replacement in /etc/systemd/system, this directory is the safest and most flexible way to do this. If you wish to override only specific directives from the system’s unit file, you can actually provide unit file snippets within a subdirectory. These will append or modify the directives of the system’s copy, allowing you to specify only the options you want to change

There is also a location for run-time unit definitions at /run/systemd/system. Unit files found in this directory have a priority landing between those in /etc/systemd/system and /lib/systemd/system. Files in this location are given less weight than the former location, but more weight than the latter.

### Types of Units
Systemd categories units according to the type of resource they describe. 
1. *.service: A service unit describes how to manage a service or application on the server. This will include how to start or stop the service, under which circumstances it should be automatically started, and the dependency and ordering information for related software.
2. *.socket: A socket unit file describes a network or IPC socket, or a FIFO buffer that systemd uses for socketbased activation. These always have an associated  service file that will be started when activity is seen on the socket that this unit defines.
3. *.device: A unit that describes a device that has been designated as needing systemd management by udev or the sysfs filesystem. Not all devices will have .device files. Some scenarios where .device units may be necessary are for ordering, mounting, and accessing the devices.
4. *.mount: This unit defines a mountpoint on the system to be managed by systemd. These are named after the mount path, with slashes changed to dashes. Entries within /etc/fstab can have units created automatically.
5. *.automount: An .automount unit configures a mountpoint that will be automatically mounted. These must be named after the mount point they refer to and must have a matching .mount unit to define the specifics of the mount.
6. *.swap: This unit describes swap space on the system. The name of these units must reEect the device or file
path of the space.
7. *.target: A target unit is used to provide synchronization points for other units when booting up or changing states. They also can be used to bring the system to a new state. Other units specify their relation to targets to become tied to the target’s operations.
8. *.path: This unit defines a path that can be used for path-based activation. By default, a .service unit of the same base name will be started when the path reaches the specified state. This uses inotify to monitor the path for changes.
9. *.timer: A .timer unit defines a timer that will be managed by systemd, similar to a cron job for delayed or scheduled activation. A matching unit will be started when the timer is reached.
10. *.snapshot: A .snapshot unit is created automatically by the systemctl snapshot command. It allows you to reconstruct the current state of the system after making changes. Snapshots do not survive across sessions and are used to roll back temporary states.
11. *.slice: A .slice unit is associated with Linux Control Group nodes, allowing resources to be restricted or assigned to any processes associated with the slice. The name reEects its hierarchical position within the cgroup tree. Units are placed in certain slices by default depending on their type.
12. *.scope: Scope units are created automatically by systemd from information received from its bus interfaces. These are used to manage sets of system processes that are created externally


### Anatomy of a Unit File
The internal structure of unit files are organized with sections. Each section extends until the beginning of the subsequent section or until the end of the file.
Section names are well defined and case-sensitive. 
```
[Section]
Directive1=value
Directive2=value
...
```

#### [Unit] section
Unit section is generally used for defining metadata for the unit and configuring the relationship of the unit to other units.
Common directives:

Description=: 
    This directive can be used to describe the name and basic functionality of the unit.

Documentation=: 
    This directive provides a location for a list of URIs for documentation. These can be either internally available man pages or web accessible URLs. 

Requires=: 
    This directive lists any units upon which this unit essentially depends. If the current unit is activated, the units listed here must successfully activate as well, else this unit will fail. These units are started in parallel with the current unit by default.

Wants=: 
    This directive is similar to Requires=, but less strict. Systemd will attempt to start any units listed here when this unit is activated. If these units are not found or fail to start, the current unit will continue to function. This is the recommended way to configure most dependency relationships. Again, this implies a parallel activation unless modified by other directives.

BindsTo=: 
    This directive is similar to Requires=, but also causes the current unit to stop when the associated unit terminates.

Before=: 
    The units listed in this directive will not be started until the current unit is marked as started if they are activated at the same time. This does not imply a dependency relationship and must be used in conjunction with one of the above directives if this is desired.

After=: 
    The units listed in this directive will be started before starting the current unit. This does not imply a dependency relationship and one must be established through the above directives if this is required.

Conflicts=: 
    This can be used to list units that cannot be run at the same time as the current unit. Starting a unit with this relationship will cause the other units to be stopped.

Condition...=: 
    There are a number of directives that start with Condition which allow the administrator to test certain conditions prior to starting the unit. This can be used to provide a generic unit file that will only be run when on appropriate systems. If the condition is not met, the unit is gracefully skipped.

Assert...=: 
    Similar to the directives that start with Condition, these directives check for different aspects of the running environment to decide whether the unit should activate. However, unlike the Condition directives, a negative result causes a failure with this directive.

#### [Install] section
Install section is often the last section, this section is optional and is used to define the behavior or a unit if it is enabled or disabled. Enabling a unit marks it to be automatically started at boot. In essence, this is accomplished by latching the unit in question onto another unit that is somewhere in the line of units to be started at boot.
Only units that can be enabled will have this section. The directives within dictate what should happen when the unit is enabled

Common directives:

WantedBy=: 
    The WantedBy= directive is the most common way to specify how a unit should be enabled. This directive allows you to specify a dependency relationship in a similar way to the Wants= directive does in the [Unit] section. 

RequiredBy=: 
    This directive is very similar to the WantedBy= directive, but instead specifies a required dependency that will cause the activation to fail if not met. When enabled, a unit with this directive will create a directory ending with .requires.

Alias=: 
    This directive allows the unit to be enabled under another name as well. Among other uses, this allows multiple providers of a function to be available, so that related units can look for any provider of the common aliased name

Also=: 
    This directive allows units to be enabled or disabled as a set. Supporting units that should always be available when this unit is active can be listed here. They will be managed as a group for installation tasks.

DefaultInstance=: 
    For template units (covered later) which can produce unit instances with unpredictable names, this can be used as a fallback value for the name if an appropriate name is not provided.


#### Unit-Specific Section Directives

Most unit types offer directives that only apply to their specific type. These are available within sections named after their type
The device, target, snapshot, and scope unit types have no unit-specific directives, and thus have no associated sections for their type.

##### [Service] section

Type= categorizes services by their process and daemonizing behavior.
Type= directive can be one of the following:

    simple: 
        The main process of the service is specified in the start line. This is the default if the Type= and Busname= directives are not set, but the ExecStart= is set. Any communication should be handled outside of the unit through a second unit of the appropriate type (like through a .socket unit if this unit must communicate using sockets).

    forking: 
        This service type is used when the service forks a child process, exiting the parent process almost immediately. This tells systemd that the process is still running even though the parent exited.

    oneshot: 
        This type indicates that the process will be short-lived and that systemd should wait for the process to exit before continuing on with other units. This is the default Type= and ExecStart= are not set. It is used for one-off tasks.

    dbus: 
        This indicates that unit will take a name on the D-Bus bus. When this happens, systemd will continue toprocess the next unit.

    notify: 
        This indicates that the service will issue a notification when it has finished starting up. The systemdprocess will wait for this to happen before proceeding to other units.

    idle: 
        This indicates that the service will not be run until all jobs are dispatched.


Some additional directives may be needed when using certain service types

RemainAfterExit=: 
    This directive is commonly used with the oneshot type. It indicates that the service should be considered active even after the process exits.

PIDFile=: 
    If the service type is marked as “forking”, this directive is used to set the path of the file that should contain the process ID number of the main child that should be monitored.

BusName=: 
    This directive should be set to the D-Bus bus name that the service will attempt to acquire when using the “dbus” service type.

NotifyAccess=: 
    This specifies access to the socket that should be used to listen for notifications when the “notify” service type is selected This can be “none”, “main”, or “all. The default, “none”, ignores all status messages. The “main” option will listen to messages from the main process and the “all” option will cause all members of the service’s control group to be processed.

Directives to manage our services

ExecStart=: 
    This specifies the full path and the arguments of the command to be executed to start the process. This may only be specified once (except for “oneshot” services). If the path to the command is preceded by a dash “-” character, non-zero exit statuses will be accepted without marking the unit activation as failed.

ExecStartPre=: 
    This can be used to provide additional commands that should be executed before the main process is started. This can be used multiple times. Again, commands must specify a full path and they can be preceded by “-” to indicate that the failure of the command will be tolerated.

ExecStartPost=: 
    This has the same exact qualities as ExecStartPre= except that it specifies commands that will be run after the main process is started.

ExecReload=: 
    This optional directive indicates the command necessary to reload the configuration of the service if available.

ExecStop=: 
    This indicates the command needed to stop the service. If this is not given, the process will be killed immediately when the service is stopped.

ExecStopPost=: 
    This can be used to specify commands to execute following the stop command.

RestartSec=: 
    If automatically restarting the service is enabled, this specifies the amount of time to wait before attempting to restart the service.

Restart=: 
    This indicates the circumstances under which systemd will attempt to automatically restart the service. This can be set to values like “always”, “on-success”, “on-failure”, “on-abnormal”, “on-abort”, or “onwatchdog”. These will trigger a restart according to the way that the service was stopped.

TimeoutSec=: 
    This configures the amount of time that systemd will wait when stopping or stopping the service before marking it as failed or forcefully killing it. You can set separate timeouts with TimeoutStartSec= and TimeoutStopSec= as well.

##### [Socket] section
Each socket unit must have a matching service unit that will be activated when the socket receives activity.

By breaking socket control outside of the service itself, sockets can be initialized early and the associated services can often be started in parallel. By default, the socket name will attempt to start the service of the same name upon receiving a connection. When the service is initialized, the socket will be passed to it, allowing it to begin processing any buffered requests.

Common directives:

ListenStream=: 
    This defines an address for a stream socket which supports sequential, reliable communication. Services that use TCP should use this socket type.

ListenDatagram=: 
    This defines an address for a datagram socket which supports fast, unreliable communication packets. Services that use UDP should set this socket type.

ListenSequentialPacket=: 
    This defines an address for sequential, reliable communication with max length datagrams that preserves message boundaries. This is found most often for Unix sockets.

ListenFIFO: 
    Along with the other listening types, you can also specify a FIFO buffer instead of a socket.

Additional directives:

Accept=: 
    This determines whether an additional instance of the service will be started for each connection. If set to false (the default), one instance will handle all connections.

SocketUser=: 
    With a Unix socket, specifies the owner of the socket. This will be the root user if left unset.

SocketGroup=: 
    With a Unix socket, specifies the group owner of the socket. This will be the root group if neither this or the above are set. If only the SocketUser= is set, systemd will try to find a matching group.

SocketMode=: 
    For Unix sockets or FIFO buffers, this sets the permissions on the created entity.

Service=: 
    If the service name does not match the .socket name, the service can be specified with this directive.

##### [Mount] section
Mount units allow for mount point management from within systemd. Mount points are named after the directory that they control, with a translation algorithm applied.

Mount units are often translated directly from /etc/fstab files during the boot process. For the unit definitions automatically created and those that you wish to define in a unit file, the following directives are useful:

What=: 
    The absolute path to the resource that needs to be mounted.

Where=: 
    The absolute path of the mount point where the resource should be mounted. This should be the same as the unit file name, except using conventional filesystem notation.

Type=: 
    The filesystem type of the mount.

Options=: 
    Any mount options that need to be applied. This is a comma-separated list.

SloppyOptions=: 
    A boolean that determines whether the mount will fail if there is an unrecognized mount option.

DirectoryMode=: 
    If parent directories need to be created for the mount point, this determines the permission mode of these directories.

TimeoutSec=: 
    Configures the amount of time the system will wait until the mount operation is marked as failed.

##### [Automount] section
This unit allows an associated .mount unit to be automatically mounted at boot. As with the .mount unit, these units must be named after the translated mount point’s path.

Only the following two options allowed:

Where=: 
    The absolute path of the automount point on the filesystem. This will match the filename except that it uses conventional path notation instead of the translation.

DirectoryMode=: 
    If the automount point or any parent directories need to be created, this will determine the permissions settings of those path components.

##### [Swap] section

Swap units are used to configure swap space on the system. The units must be named after the swap file or the swap device, using the same filesystem translation that was discussed above.

Like the mount options, the swap units can be automatically created from /etc/fstab entries, or can be configured through a dedicated unit file

Directives:

What=: 
    The absolute path to the location of the swap space, whether this is a file or a device.

Priority=: 
    This takes an integer that indicates the priority of the swap being configured.

Options=: 
    Any options that are typically set in the /etc/fstab file can be set with this directive instead. A commaseparated list is used.

TimeoutSec=: 
    The amount of time that systemd waits for the swap to be activated before marking the operation as a failure.

##### [Path] section

A path unit defines a filesystem path that systmed can monitor for changes. Another unit must exist that will be be activated when certain activity is detected at the path location. Path activity is determined thorugh inotify events.

Directives:

PathExists=: 
    This directive is used to check whether the path in question exists. If it does, the associated unit is activated.

PathExistsGlob=: 
    This is the same as the above, but supports file glob expressions for determining path existence.

PathChanged=: 
    This watches the path location for changes. The associated unit is activated if a change is detected when the watched file is closed.

PathModified=: 
    This watches for changes like the above directive, but it activates on file writes as well as when the file is closed.

DirectoryNotEmpty=: 
    This directive allows systemd to activate the associated unit when the directory is no longer empty.

Unit=: 
    This specifies the unit to activate when the path conditions specified above are met. If this is omitted, systemd will look for a .service file that shares the same base unit name as this unit.

MakeDirectory=: 
    This determines if systemd will create the directory structure of the path in question prior to watching.

DirectoryMode=: 
    If the above is enabled, this will set the permission mode of any path components that must be created.

##### [Timer] section

Timer units are used to schedule tasks to operate at a specific time or after a certain delay. This unit type replaces or supplements some of the functionality of the cron and at daemons. An associated unit must be provided which will be activated when the timer is reached.

Directives:


OnActiveSec=: 
    This directive allows the associated unit to be activated relative to the .timer unit’s activation.

OnBootSec=: 
    This directive is used to specify the amount of time after the system is booted when the associated unit should be activated.

OnStartupSec=: 
    This directive is similar to the above timer, but in relation to when the systemd process itself was started.

OnUnitActiveSec=: 
    This sets a timer according to when the associated unit was last activated.

OnUnitInactiveSec=: 
    This sets the timer in relation to when the associated unit was last marked as inactive.

OnCalendar=: 
    This allows you to activate the associated unit by specifying an absolute instead of relative to an event.

AccuracySec=: 
    This unit is used to set the level of accuracy with which the timer should be adhered to. By default, the associated unit will be activated within one minute of the timer being reached. The value of this directive will determine the upper bounds on the window in which systemd schedules the activation to occur.

Unit=: 
    This directive is used to specify the unit that should be activated when the timer elapses. If unset, systemd will look for a .service unit with a name that matches this unit.

Persistent=: 
    If this is set, systemd will trigger the associated unit when the timer becomes active if it would have been triggered during the period in which the timer was inactive.

WakeSystem=: 
    Setting this directive allows you to wake a system from suspend if the timer is reached when in that state.

##### [Slice] section
The [Slice] section of a unit file actually does not have any .slice unit specific configuration. Instead, it can contain some resource management directives that are actually available to a number of the units listed above.

Some common directives in the [Slice] section, which may also be used in other units can be found in the systemd.resource-control man page. These are valid in the following unit-specific sections:
[Slice]
[Scope]
[Service]
[Socket]
[Mount]
[Swap]

### Creating Instance Units from Template Unit Files

Template unit files are, in most ways, no different than regular unit files. However, these provide flexibility in configuring units by allowing certain parts of the file to utilize dynamic information that will be available at runtime.

#### Template and Instance Unit Names
Template unit files can be identified because they contain an @ symbol after the base unit name and before the unit type suffix
example@.service

When an instance is created from a template, an instance identifier is placed between the @ symbol and the period signifying the start of the unit type.
example@instance1.service

An instance file is usually created as a symbolic link to the template file, with the link name including the instance identifier. In this way,
multiple links with unique identifiers can point back to a single template file. When managing an instance unit, systemd will look for a file
with the exact instance name you specify on the command line to use. If it cannot find one, it will look for an associated template file.

#### Template Specifiers
The power of template unit files is mainly seen through its ability to dynamically substitute appropriate information within the unit definition according to the operating environment. This is done by setting the directives in the template file as normal, but replacing certain values or parts of values with variable specifiers.

The following are some of the more common specifiers will be replaced when an instance unit is interpreted with the relevant information:

%n: 
    Anywhere where this appears in a template file, the full resulting unit name will be inserted.
%N: 
    This is the same as the above, but any escaping, such as those present in file path patterns, will be reversed.
%p: 
    This references the unit name prefix. This is the portion of the unit name that comes before the @ symbol.
%P: 
    This is the same as above, but with any escaping reversed.
%i: 
    This references the instance name, which is the identifier following the @ in the instance unit. This is one of the most commonly used specifiers because it will be guaranteed to be dynamic. The use of this identifier encourages the use of configuration significant identifiers. For example, the port that the service will be run at can be used as the instance identifier and the template can use this specifier to set up the port specification.
%I: 
    This specifier is the same as the above, but with any escaping reversed.
%f: 
    This will be replaced with the unescaped instance name or the prefix name, prepended with a /.
%c: 
    This will indicate the control group of the unit, with the standard parent hierarchy of /sys/fs/cgroup/ssytemd/ removed.
%u: 
    The name of the user configured to run the unit.
%U: 
    The same as above, but as a numeric UID instead of name.
%H: 
    The host name of the system that is running the unit.
%%: 
    This is used to insert a literal percentage sign.

By using the above identifiers in a template file, systemd will fill in the correct values when interpreting the template to create an instance
unit.

## Unit Management

start the service:
    systemctl start nginx.service

can stop the service:
    systemctl stop nginx.service

restart the service:
    systemctl restart nginx.service

attempt to reload the service without interrupting normal functionality:
    systemctl reload nginx.service


### Enabling or Disabling Units
By default, most systemd unit files are not started automatically at boot. To configure this functionality, you need to “enable” to unit. This hooks it up to a certain boot “target”, causing it to be triggered when that target is started.

enable a service to start automatically at boot:
    systemctl enable nginx.service

disable the service:
    systemctl disable nginx.service

### Getting an Overview of the System State

get all of the unit files that systemd has listed as “active”:
    systemctl list-units

list all of the units that systemd has loaded or attempted to load into memory, including those that are not currently active:
    systemctl list-units --all

list all of the units installed on the system, including those that systemd has not tried to load into memory:
    systemctl list-unit-files

### Viewing Basic Log Information
A systemd component called journald collects and manages journal entries from all parts of the system. This is basically log information from applications and the kernel

see all log entries, starting at the oldest entry:
    journalctl

see the journal entries from the current boot:
    journalctl -b

see only kernel messages:
    journalctl -k

see only kernel messages for the current boot:
    journalctl -k -b


### Querying Unit States and Logs

see an overview of the current state of a unit:
    systemctl status nginx.service

see all of the journal entries for the unit in question:
    journalctl -u nginx.service

limit the entries to the current boot:
    journalctl -b -u nginx.service

### Inspecting Units and Unit Files
A unit file contains the parameters that systemd uses to manage and run a unit.

see the full contents of a unit file:
    systemctl cat nginx.service

see the dependency tree of a unit (which units systemd will attempt to activate when starting the unit):
    systemctl list-dependencies nginx.service

expand all dependent units recursively:
    systemctl list-dependencies --all nginx.service

see the low-level details of the unit’s settings on the system(give you the value of each parameter being managed by systemd):
    systemctl show nginx.service

### Modifying Unit Files

add a unit file snippet:
    systemctl edit nginx.service

modify the entire content of the unit file:
    systemctl edit --full nginx.service

After modifying a unit file, you should reload the systemd process itself to pick up your changes:
    systemctl daemon-reload

### Using Targets (Runlevels)
Targets are basically synchronization points that the server can used to bring the server into a specific state. Service and other unit files can be tied to a target and multiple targets can be active at the same time.

see all of the targets available on your system:
    systemctl list-unit-files --type=target

view the default target that systemd tries to reach at boot (which in turn starts all of the unit files that make up the dependency tree of
that target):
    systemctl get-default

change the default target that will be used at boot:
    systemctl set-default multi-user.target

see what units are tied to a target:
    systemctl list-dependencies multi-user.target

You can modify the system state to transition between targets with the isolate option. This will stop any units that are not tied to the specified target. Be sure that the target you are isolating does not stop any essential services:
    systemctl isolate multi-user.target

### Stopping or Rebooting the Server
Shortcuts are available for some of the major states that a system can transition to

power off your server
    systemctl poweroff

reboot the system
    systemctl reboot

boot into rescue mode
    systemctl rescue

### Starting and Stopping Services

start a systemd service
    systemctl start application.service
or
    systemctl start application

stop a currently running service
    sudo systemctl stop application.service

### Restarting and Reloading

restart a running service
    systemctl restart application.service

If the application in question is able to reload its configuration files (without restarting), you can issue the reload command to initiate that
process:
    systemctl reload application.service

If you are unsure whether the service has the functionality to reload its configuration, you can issue the reload-or-restart command. This
will reload the configuration in-place if available. Otherwise, it will restart the service so the new configuration is picked up:
    systemctl reload-or-restart application.service

### Enabling and Disabling Services

start a service at boot (Keep in mind that enabling a service does not start it in the current session)
    systemctl enable application.service

disable the service from starting automatically
    systemctl disable application.service

### Checking the Status of Services

check the status of a service on your system
    systemctl status application.service

check to see if a unit is currently active (running)
    systemctl is-active application.service

see if the unit is enabled
    systemctl is-enabled application.service

check whether the unit is in a failed state
    systemctl is-failed application.service

### Listing Current Units

see a list of all of the active units that systemd knows about
    systemctl list-units
or
    systemctl

see all of the units that systemd has loaded (or attempted to load), regardless of whether they are currently active:
    systemctl list-units --all

use other flags to filter these results, for example:
    systemctl list-units --all --state=inactive

only display units of the type we are interested in:
    systemctl list-units --type=service

### Listing All Unit Files

see every available unit file within the systemd paths, including those that systemd has not attempted to load:
    systemctl list-unit-files

### Displaying a Unit File

see the unit file:
    systemctl cat atd.service

### Displaying Dependencies

display a hierarchy mapping the dependencies that must be dealt with in order to start the unit in question:
    systemctl list-dependencies sshd.service

to recursively list all dependencies, include the –all flag:
    systemctl list-dependencies sshd.service -all

To show reverse dependencies (units that depend on the specified unit), add the –reverse Eag to the command:
    systemctl list-dependencies sshd.service -reverse

show units that depend on the specified unit starting before and after themselves:
    systemctl list-dependencies sshd.service -before
    systemctl list-dependencies sshd.service -after

### Checking Unit Properties

display a list of properties that are set for the specified unit using a key=value format:
    systemctl show sshd.service

display a single property, you can pass the -p flag with the property name, for example:
    systemctl show sshd.service -p Conflicts
    (Output: Conflicts=shutdown.target)

### Masking and Unmasking Units

Systemd also has the ability to mark a unit as completely unstartable, automatically or manually, by linking it to /dev/null. 
This is called masking the unit, and is possible with the mask command:
    systemctl mask nginx.service

the service will be listed as masked through command:
    systemctl list-unit-files

unmask a unit:
    systemctl unmask nginx.service

### Editing Unit Files

The edit command, by default, will open a unit file snippet for the unit in question:
    systemctl edit nginx.service

edit the full unit file instead of creating a snippet:
    systemctl edit --full nginx.service

to remove a snippet:
    rm -r /etc/systemd/system/nginx.service.d

to remove a full modified unit file:
    rm /etc/systemd/system/nginx.service

After deleting the file or directory, you should reload the systemd process so that it no longer attempts to reference these files and reverts
back to using the system copies:
    systemctl daemon-reload

### Adjusting the System State (Runlevel) with Targets

Targets are special unit les that describe a system state or synchronization point. Targets do not do much themselves, but are instead used to group other units together.

This can be used in order to bring the system to certain states, much like other init systems use runlevels. They are used as a reference for when certain functions are available, allowing you to specify the desired state instead of the individual units needed to produce that state

For instance, there is a swap.target that is used to indicate that swap is ready for use. Units that are part of this process can sync with this target by indicating in their conguration that they are WantedBy= or RequiredBy= the swap.target. Units that require swap to be available can specify this condition using the Wants=, Requires=, and After= specifications to indicate the nature of their relationship.

#### Getting and Setting the Default Target

find the default target for your system:
    systemctl get-default

set a different default target:
    systemctl set-default graphical.target


#### Listing Available Targets

Unlike runlevels, multiple targets can be active at one time. An active target indicates that systemd has attempted to start all of the units tied to the target and has not tried to tear them down again

get a list of the available targets on your system:
    systemctl list-unit-files --type=target

see all of the active targets:
    systemctl list-units --type=target

#### Isolating Targets

It is possible to start all of the units associated with a target and stop all units that are not part of the dependency tree. This is similar to changing the runlevel in other init systems.

For instance, if you are operating in a graphical environment with graphical.target active, you can shut down the graphical system and put the system into a multi-user command line state by isolating the multi-user.target. Since graphical.target depends on multi-user.target but not the other way around, all of the graphical units will be stopped.

You may wish to take a look at the dependencies of the target you are isolating before performing this procedure to ensure that you are not stopping vital services:
    systemctl list-dependencies multi-user.target
When you are satisfied with the units that will be kept alive, you can isolate the target:
    systemctl isolate multi-user.target

### Using Shortcuts for Important Events

There are targets defined for important events like powering off or rebooting. However, systemctl also has some shortcuts that add a bit of additional functionality.


put the system into rescue (single-user) mode, you can just use the rescue command instead of isolate rescue.target:
    systemctl rescue
This will provide the additional functionality of alerting all logged in users about the event.

halt the system
    systemctl halt

initiate a full shutdown
    systemctl poweroff

A restart can be started with the reboot command:
    systemctl reboot

### View and Manipulate Systemd Logs

Systemd provides a centralized management solution for logging all kernel and userland processes. The system that collects and manages these logs is known as the journal.

The journald daemon collects data from all available sources and stores them in a binary format for easy and dynamic manipulation.

#### Setting the System Time

By default, systemd will display results in local time.

see what timezones are available:
    timedatectl list-timezones

set timezone for log:
    sudo timedatectl set-timezone zone

check that your machine is using the correct time now:
    timedatectl status

#### Basic Log Viewing

display every journal entry that is in the system:
    journalctl

display the timestamps in UTC:
    journalctl --utc

#### Journal Filtering by Time

Displaying Logs from the Current Boot:
    journalctl -b

Past Boots:
Some distributions enable saving previous boot information by default, while others disable this feature. To enable persistent boot information, you can either create the directory to store the journal:
    sudo mkdir -p /var/log/journal
Or you can edit the journal configuration file:
    sudo vim /etc/systemd/journald.conf
    Under the [Journal] section, set the Storage= option to “persistent” to enable persistent logging:
        [Journal]
        Storage=persistent

see the boots that journald knows about:
    journalctl --list-boots

To display information from these boots, you can use information from either the first or second column:
    For instance, to see the journal from the previous boot, use the -1 relative pointer with the -b flag:
        journalctl -b -1
    You can also use the boot ID to call back the data from a boot:
        journalctl -b caf0524a1d394ce0bdbcff75b94444fe

Time Windows:
    restrict the entries displayed to those after or before the given time:
        journalctl --since "2015-06-01 01:00:00"
        journalctl --since "2015-06-01" --until "2015-06-13 15:00"
        journalctl --since yesterday
        journalctl --since 09:00 --until "1 hour ago"

#### Filtering by Message Interest

##### By Unit
    journalctl -u nginx.service
    journalctl -u nginx.service --since today
    journalctl -u nginx.service -u php-fpm.service --since today

##### By Process, User, or Group ID
    journalctl _PID=8088
    journalctl _UID=33 --since today (user ID can be found through id -u www-data (www-data is user name))
find out about all of the available journal fields:
    man systemd.journal-fields

##### By Component Path

find those entries that involve the bash executable:
    journalctl /usr/bin/bash

##### Displaying Kernel Messages
    journalctl -k
    journalctl -k -b -5 (get the messages from five boots ago)

### By Priority

show only entries logged at the error level or above:
    journalctl -p err -b

### Modifying the Journal Display

Truncate or Expand Output:
    journalctl --no-full (truncate the output)
    journalctl -a (o display all of its information)

#### Output to Standard Out

By default, journalctl displays output in a pager for easier consumption. If you are planning on processing the data with text manipulation tools, however, you probably want to be able to output to standard output.

    journalclt --no-pager

This can be piped immediately into a processing utility or redirected into a file on disk, depending on your needs

#### Output Formats

output the journal entries in JSON:
    journalctl -b -u nginx -o json
    journalctl -b -u nginx -o json-pretty

### Active Process Monitoring
#### Displaying Recent Logs
    journalctl -n (display the most recent 10 entries)
    journalctl -n 20

    Following Logs:
        journalctl -f

### Journal Maintenance
    find out the amount of space that the journal is currently occupying on disk:
        journalctl --disk-usage

    Deleting Old Logs:
        1. shrink your journal by indicating a size(remove old entries until the total journal space taken up on disk is at the requested size)
            sudo journalctl --vacuum-size=1G
        2. shrink the journal is providing a cutoff time(Any entries beyond that time are deleted)
            sudo journalctl --vacuum-time=1years

### Limiting Journal Expansion
You can configure your server to place limits on how much space the journal can take up. This can be done by editing the /etc/systemd/journald.conf file.

The following items can be used to limit the journal growth:
    SystemMaxUse=: 
        Specifies the maximum disk space that can be used by the journal in persistent storage.
    SystemKeepFree=: 
        Specifies the amount of space that the journal should leave free when adding journal entries to persistent storage.
    SystemMaxFileSize=: 
        Controls how large individual journal files can grow to in persistent storage before being rotated.
    RuntimeMaxUse=: 
        Specifies the maximum disk space that can be used in volatile storage (within the /run filesystem).
    RuntimeKeepFree=: 
        Specifies the amount of space to be set aside for other uses when writing data to volatile storage (within the /run filesystem).
    RuntimeMaxFileSize=: 
        Specifies the amount of space that an individual journal file can take up in volatile storage (within the /run filesystem) before being rotated