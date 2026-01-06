> Protocol Buffers are a language-neutral, platform-neutral extensible mechanism for serializing structured data.

[An Intro to Protocol Buffers with Python](https://www.blog.pythonlibrary.org/2023/08/30/an-intro-to-protocol-buffers-with-python/)  
[Protocol Buffers Documentation](https://protobuf.dev/)  

[Language Guide (proto 3)](https://protobuf.dev/programming-guides/proto3/)  

![Google Protocol Buffers 工作原理](./Google_Protobuf_Working_Principle.md)  

[Protocol Buffers Python API Reference](https://googleapis.dev/python/protobuf/latest/index.html)  
[]()  
[]()  

- how to use protobuf
```
                                                   -->  Networks  --
                                                   |                |                              Deserialization
       Serialization                Byte-Stream    -->  Files     --|  -->   Byte-Stream    -->   (Language Specific Object)
(Language Specific Object)  -->     | | | | | |    |                         | | | | | |
                                                   |-->  Storage  --|
```

- protoc
```
                            --cpp_out=OUT_DIR           Generate C++ header and source.
                            --csharp_out=OUT_DIR        Generate C# source file.
                            --java_out=OUT_DIR          Generate Java source file.
.proto  --->  protoc  --->  --js_out=OUT_DIR            Generate JavaScript source.
                            --objc_out=OUT_DIR          Generate Objective C header and source.
                            --php_out=OUT_DIR           Generate PHP source file.
                            --python_out=OUT_DIR        Generate Python source file.
                            --ruby_out=OUT_DIR          Generate Ruby source file.
```