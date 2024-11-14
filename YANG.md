### What it is
A **data modeling language** used to model configuration and state data manipulated by a **NETCONF agent**.
### YANG in ConfD
In ConfD, YANG is not only used for NETCONF data. On the contrary, YANG is used to ==describe the data model as a whole and used by all northbound interfaces==. 

A YANG module can be directly transformed into a **final schema (.==fxs==) file** that can be loaded into ConfD.
### Module
A module contains **three** types of statements
1. **module-header statements**: describe the module itself
2. **revision statements**: information about the history of the module
3. **definition statements**: body of the module where the data model is defined
**include** statement: to reference material in submodules
**import** statement: references to material defined in other modules
#### Data Modeling Basics
##### leaf
like an integer or a string, has exactly one value and no child nodes
```json
leaf host-name {
	type string;
	description "Hostname for this system";
}

`XML example`:
my.example.com
```
##### leaf-list
a sequence of leaf nodes with exactly one value of a particular type per leaf
```json
leaf-list domain-search {
	type string;
	description "List of domain names to search";

`XML example`:
<domain-search>high.example.com</domain-search>
<domain-search>low.example.com</domain-search>
<domain-search>everywhere.example.com</domain-search>
```
##### container
group related nodes in a subtree, has only child nodes and no value
```json
container system {
	container login {
leaf message {
	type string;
	description "Message given at start of login session";
}
	}
}


`XML example`:
<system>
	<login>
<message>Good morning, Dave</message>
	</login>
</system>
```
##### list
a sequence of list entries, Each entry is uniquely identified by the values of its key leafs
```json
list user {
	key "name";
	leaf name {
type string;
	}
	leaf full-name {
type string;
	}
	leaf class {
type string;
	}
}


`XML example`:
<user>
	<name>glocks</name>
	<full-name>Goldie Locks</full-name>
	<class>intruder</class>
</user>
<user>
	<name>snowey</name>
	<full-name>Snow White</full-name>
	<class>free-loader</class>
</user>
<user>
	<name>rzull</name>
	<full-name>Repun Zell</full-name>
	<class>tower</class>
</user>
```
##### state data
- `config false`: its sub hierarchy is flagged as state data, to be reported using NETCONF's get operation, not the get-config operation
- `config false`: manipulated using get-config or edit-config.
  ```json
  list interface {
  	key "name";
  	config true;
  
  	leaf name {
  type string;
  	}
  	leaf speed {
  type enumeration {
  	enum 10m;
  	enum 100m;
  	enum auto;
  }
  	}
  	leaf observed-speed {
  type uint32;
  config false;
  	}
  }
  ```
##### Built-in Types
	- binary
	- boolean
	- empty
	- enumeration
	- uint32
##### Derived Types (typedef)
```json
typedef percent {
	type uint16 {
range "0 .. 100";
	}
	description "Percentage";
}

leaf completed {
	type percent;
}

`XML example`:
<completed>20</completed>
```
##### Reusable Node Groups (grouping)
grouping defines a set of nodes that are instantiated with the uses statement, The grouping can be ==refined== as it is used, allowing certain statements to be ==overridden==
```json
grouping target {
	leaf address {
type inet:ip-address;
description "Target IP address";
	}
	leaf port {
type inet:port-number;
description "Target port number";
	}
}

container peer {
	container destination {
uses target;
	}
}

`The grouping can be refined as it is used, allowing certain statements to be overridden.`
container connection {
	container source {
uses target {
	refine "address" {
		description "Source IP address";
	}
	refine "port" {
		description "Source port number";
	}
}
	}
	container destination {
uses target {
	refine "address" {
		description "Destination IP address";
	}
	refine "port" {
		description "Destination port number";
	}
}
	}
}


`XML example`:
<peer>
	<destination>
<address>192.0.2.1</address>
<port>830</port>
	</destination>
</peer>
```
##### Choices
segregate incompatible nodes into distinct choices using the choice and case statements
```json
container food {
	choice snack {
mandatory true;
case sports-arena {
	leaf pretzel {
		type empty;
	}
	leaf beer {
		type empty;
	}
}
case late-night {
	leaf chocolate {
		type enumeration {
			enum dark;
			enum milk;
			enum first-available;
		}
	}
}
	}
}


`XML example`:
<food>
	<chocolate>first-available</chocolate>
</food>
```
##### Extending Data Models (augment)
insert additional nodes into data models, including both the current module (and its submodules) or an external module.  The `augment` statement defines the location in the data model hierarchy where new nodes are inserted, and the `when` statement defines the conditions when the new nodes are valid.
```json
`defines a uid node that only is valid when the user's class is not wheel`
augment /system/login/user {
	when "class != 'wheel'";
	leaf uid {
type uint16 {
	range "1000 .. 30000";
}
	}
}


`XML example`:
<user>
	<name>alicew</name>
	<full-name>Alice N. Wonderland</full-name>
	<class>drop-out</class>
	<other:uid>1024</other:uid>
</user>
```
##### RPC Definitions
definition of NETCONF RPCs, method names, input parameters and output parameters are modeled
```json
rpc activate-software-image {
	input {
leaf image-name {
	type string;
}
	}
	output {
leaf status {
	type string;
}
	}
}

`XML example`:
<rpc message-id="101"
xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
	<activate-software-image xmlns="http://acme.example.com/system">
<name>acmefw-2.3</name>
	</activate-software-image>
</rpc>

<rpc-reply message-id="101"
      xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
	<status xmlns="http://acme.example.com/system">
The image acmefw-2.3 is being installed.
	</status>
</rpc-reply>
```
##### Notification Definitions
notifications suitable for NETCONF
```json
notification link-failure {
	description "A link failure has been detected";
	leaf if-name {
type leafref {
	path "/interfaces/interface/name";
}
	}
	leaf if-admin-status {
type ifAdminStatus;
	}
}


`XML example`:
<notification xmlns="urn:ietf:params:netconf:capability:notification:1.0">
	<eventTime>2007-09-01T10:00:00Z</eventTime>
	<link-failure xmlns="http://acme.example.com/system">
<if-name>so-1/2/3.0</if-name>
<if-admin-status>up</if-admin-status>
	</link-failure>
</notification>
```
##### Integrity Constraints
built-in declarative constructs for common integrity constraints
- **must** statement
- **unique** statement
  ```json
  container interface {
  	leaf ifType {
  type enumeration {
  	enum ethernet;
  	enum atm;
  }
  	}
  	leaf ifMTU {
  type uint32;
  	}
  	must "ifType != 'ethernet' or "
   + "(ifType = 'ethernet' and ifMTU = 1500)" {
     error-message "An ethernet MTU must be 1500";
  	}
  	must "ifType != 'atm' or "
   + "(ifType = 'atm' and ifMTU <= 17966 and ifMTU >= 64)" {
     error-message "An atm MTU must be 64 .. 17966";
  	}
  }
  
  list server {
  	key "name";
  	unique "ip port";
  	leaf name {
  type string;
  	}
  	leaf ip {
  type inet:ip-address;
  	}
  	leaf port {
  type inet:port-number;
  	}
  }
  
  
  `XML example(NOT VALID)`:
  <server>
  	<name>smtp</name>
  	<ip>192.0.2.1</ip>
  	<port>25</port>
  </server>
  
  <server>
  	<name>http</name>
  	<ip>192.0.2.1</ip>
  	<port>25</port>
  </server>
  ```
##### when statement
used to make its parent statement conditional
```json
leaf a {
	type boolean;
}
leaf b {
	type string;
	when "../a = 'true'";
}

`This data model snippet says that 'b' can only exist if 'a' is true. If 'a' is true, and 'b' has a value, and 'a' is set to false, 'b' will automatically be deleted`
```
##### Tail-f Extensions with YANG
to integrate YANG models in ConfD
- tailf:callpoint : annotate the YANG model with a Tail-f extension to invoke user C code to deliver the statistics data in runtime
- tailf:validate : to invoke user code to validate the configuration
  ```json
  module test {
  	namespace "http://tail-f.com/test";
  	prefix "t";
  	import ietf-inet-types {
  prefix inet;
  	}
  	import tailf-common {
  prefix tailf;
  	}
  
  	container top {
  leaf a {
  	type int32;
  	config false;
  	`callpoint`
  	tailf:callpoint mycp;
  }
  leaf b {
  	`validation point`
  	tailf:validate myvalcp {
  		tailf:dependency "../a";
  	}
  	type string;
  }
  leaf ip {
  	type inet:ipv4-address;
  }
  	}
  }
  ```
##### YANG annotation file
to keep the Tail-f extension statements in a separate annotation file
A YANG annotation file is a normal YANG module which imports the module to annotate. Then the `tailf:annotate` statement is used to annotate nodes in the original module.
```json
module test {
	namespace "http://tail-f.com/test";
	prefix "t";

	import ietf-inet-types {
prefix inet;
	}

	container top {
leaf a {
	type int32;
	config false;
}
leaf b {
	type string;
}
leaf ip {
	type inet:ipv4-address;
}
	}
}

module test-ann {
	namespace "http://tail-f.com/test-ann";
	prefix "ta";

	import test {
prefix t;
	}
	import tailf-common {
prefix tailf;
	}

	tailf:annotate "/t:top/t:a" {
tailf:callpoint mycp;
	}
	tailf:annotate "/t:top" {
tailf:annotate "t:b" { // recursive annotation
	tailf:validate myvalcp {
		tailf:dependency "../t:a";
	}
}
	}
}
```
compile the module with annotations `confdc -c -a test-ann.yang test.yang`
### Example: Modeling a List of Interfaces
#### Information we have to model
```sh
$ /sbin/ip link list
1: eth0: <BROADCAST,MULTICAST,UP>; mtu 1500 qdisc pfifo_fast qlen 1000
 link/ether 00:12:3f:7d:b0:32 brd ff:ff:ff:ff:ff:ff
2: lo: <LOOPBACK,UP>; mtu 16436 qdisc noqueue
 link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
3: dummy0: <BROADCAST,NOARP> mtu 1500 qdisc noop
 link/ether a6:17:b9:86:2c:04 brd ff:ff:ff:ff:ff:ff
```
#### how we want to represent the above in XML
```html
<?xml version="1.0"?>
	<config xmlns="http://example.com/ns/link">
<links>
	<link>
	<name>eth0</name>
	<flags>
		<UP />
		<BROADCAST />
		<MULTICAST /> </flags>
	<addr>00:12:3f:7d:b0:32</addr>
	<brd>ff:ff:ff:ff:ff:ff</brd>
	<mtu>1500</mtu>
	</link>
	
	<link>
	<name>lo</name>
	<flags>
		<UP />
		<LOOPBACK /> </flags>
	<addr>00:00:00:00:00:00</addr>
	<brd>00:00:00:00:00:00</brd>
	<mtu>16436</mtu>
	</link>
</links>
	</config>
```
#### The Final Interface YANG model
```json
module links {
	`YANG model header`
	namespace "http://example.com/ns/link";
	prefix link;

	`mac-address type is defined in the ietf-yang-types.yang`
	import ietf-yang-types {
prefix yang;
	}

	`grouping can be used in more than one place`
	grouping LinkFlagsType {
leaf UP {
	type empty;
}
leaf NOARP {
	type empty;
}
leaf BROADCAST {
	type empty;
}
leaf MULTICAST {
	type empty;
}
leaf LOOPBACK {
	type empty;
}
leaf NOTRAILERS {
	type empty;
}
	}

	`define an enumeration of the different queue disciplines`
	typedef QueueDisciplineType {
type enumeration {
	enum pfifo_fast;
	enum noqueue;
	enum noop;
	enum htp;
}
	}

	container config {
container links {
	list link {
		`key declaration to guarantee unique value for this leaf. If one leaf alone does not uniquely identify an object, we can define multiple keys. At least one leaf must be an instance key - we cannot have lists without a key`
		key name;
		unique addr;
		max-elements 1024;
		leaf name {
			type string;
		}
		container flags {
			uses LinkFlagsType;
		}
		leaf addr {
			type yang:mac-address;
			mandatory true;
		}
		leaf brd {
			type yang:mac-address;
			mandatory true;
		}
		leaf mtu {
			type unit32;
			default 1500;
		}
	}
}
	}

	container queueDisciplines {
list queueDiscipline {
	key linkName;
	max-elements 1024;
	leaf linkName {
		type leafref {
			path "/config/links/link/name";
		}
	}
	leaf type {
		type QueueDisciplineType;
		mandatory true;
	}
	leaf length {
		type uint32;
	}
}
	}

	container linkLimitations {
list linkLimitation {
	key linkName;
	leaf linkName {
		type leafref {
			path "/config/links/link/name";
		}
	}
	container limitations {
		leaf only10Mbps {
			type boolean;
			default false;
		}
		leaf onlyHalfDuplex {
			type boolean;
			default false;
		}
	}
}
	}

	container defaultLink {
leaf linkName {
	type leafref {
		path "/config/links/link/name";
	}
}
	}
}
```
[pyang](https://github.com/mbj4668/pyang)  
[网络工程师的Python之路 -- NETCONF、YANG、ncclient理论和实战](https://zhuanlan.zhihu.com/p/258555515)  
[What Is YANG?](https://info.support.huawei.com/info-finder/encyclopedia/en/YANG.html)  
[NETCONF Programming Guide](https://support.huawei.com/enterprise/en/doc/EDOC1100216384)  
[Understanding YANG Introduction to the YANG modeling language](https://network.developer.nokia.com/sr/learn/yang/understanding-yang/)  
[ConfD User Guide](http://66.218.245.39/doc/html/)  
[ConfD: The external database API](http://66.218.245.39/doc/html/ch07.html)  
[confd_lib_dp — callback library for connecting data providers to ConfD](http://66.218.245.39/doc/html/rn02re11.html)  
[tailf_yang_extensions — Tail-f YANG extensions](http://66.218.245.39/doc/html/rn03re21.html)  
