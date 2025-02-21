- 常用参数
	- `-d`
		- >        -d
		         --dynamic
		  Displays the contents of the file's dynamic section, if it has one.
		- 显示程序运行的rpath路径
			- ```bash
			  morrism@localhost ~/repos/berwick-setup (develop) $ readelf -d ./build/armstrong/powerpc/bin/cms
			  Dynamic section at offset 0x44fe98 contains 37 entries:
			    Tag        Type                         Name/Value
			   0x00000001 (NEEDED)                     Shared library: [libm.so.6]
			   0x00000001 (NEEDED)                     Shared library: [librt.so.1]
			   0x00000001 (NEEDED)                     Shared library: [libxml2.so.2]
			   0x00000001 (NEEDED)                     Shared library: [libz.so.1]
			   0x00000001 (NEEDED)                     Shared library: [libssl.so.1.1]
			   0x00000001 (NEEDED)                     Shared library: [libcrypto.so.1.1]
			   0x00000001 (NEEDED)                     Shared library: [libpcap.so.1]
			   0x00000001 (NEEDED)                     Shared library: [libutil.so.1]
			   0x00000001 (NEEDED)                     Shared library: [libconfd.so]
			   0x00000001 (NEEDED)                     Shared library: [librdkafka.so.1]
			   0x00000001 (NEEDED)                     Shared library: [libpthread.so.0]
			   0x00000001 (NEEDED)                     Shared library: [libc.so.6]
			   0x0000000f (RPATH)                      Library rpath: [/opt/berwick/tailf-confd-pkg/powerpc/lib:../../../librdkafka/powerpc/lib]
			  ```
- 参考文档
	- [man readelf](https://man7.org/linux/man-pages/man1/readelf.1.html)