[XML Tutorial](https://www.w3schools.com/xml/)  
[XML Formatter](https://jsonformatter.org/xml-formatter)  
[XML Formatter](https://www.freeformatter.com/xml-formatter.html)  



- xmllint
	- xml格式化命令
		- ```bash
		  morrism@localhost /tmp/x $ xmllint --format test.xml
		  <?xml version="1.0" encoding="UTF-8"?>
		  <node name="/org/freedesktop/DBus/Properties">
		    <interface name="org.freedesktop.DBus.Properties">
		      <method name="Set">
		        <arg type="s" name="interface_name"/>
		        <arg type="s" name="property_name"/>
		        <arg type="v" name="value"/>
		      </method>
		    </interface>
		  </node>
		  ```