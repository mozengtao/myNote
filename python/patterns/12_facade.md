# Facade Pattern in Python

---

## 1. Pattern Name: Facade

**Purpose / Problem Solved:**
Provide a simplified interface to a complex subsystem. Reduces coupling between clients and subsystem components.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CLIENT                                    |
+------------------------------------------------------------------+
                              |
                              | Simple interface
                              v
+------------------------------------------------------------------+
|                         FACADE                                    |
|------------------------------------------------------------------|
| + simple_operation()                                              |
|   # Coordinates subsystem components                              |
+------------------------------------------------------------------+
          |            |            |            |
          v            v            v            v
    +----------+ +----------+ +----------+ +----------+
    | Subsys A | | Subsys B | | Subsys C | | Subsys D |
    +----------+ +----------+ +----------+ +----------+
     Complex internal dependencies hidden from client
```

**中文说明：**
外观模式为复杂子系统提供简化接口。客户端只需与Facade交互，无需了解子系统内部复杂性。Facade协调子系统组件完成客户端请求。这不是强制模式——客户端仍可直接访问子系统。常用于：API简化、遗留代码封装、第三方库集成。

---

## 3. Python Grammar & Tips

| Feature | Why It Works |
|---------|--------------|
| **Module as facade** | Module can expose simplified API |
| **`__all__`** | Control what gets exported |
| **`@property`** | Hide complex getters behind simple attributes |
| **Context managers** | Encapsulate setup/teardown complexity |

---

## 4. Python Module Example

```python
#!/usr/bin/env python3
"""Facade Pattern - Video Conversion Example"""

from dataclasses import dataclass


# ============== COMPLEX SUBSYSTEM ==============

class VideoFile:
    def __init__(self, filename: str):
        self.filename = filename
        self.codec = filename.split('.')[-1]

class CodecFactory:
    @staticmethod
    def extract(file: VideoFile) -> str:
        return f"Extracted {file.codec} codec"

class BitrateReader:
    @staticmethod
    def read(file: VideoFile) -> int:
        return 1000  # kbps

class AudioMixer:
    @staticmethod
    def fix(codec: str) -> str:
        return f"Fixed {codec} audio"

class VideoEncoder:
    @staticmethod
    def encode(source: VideoFile, target_format: str, 
               bitrate: int, audio: str) -> str:
        return f"Encoded to {target_format} at {bitrate}kbps"


# ============== FACADE ==============

class VideoConverter:
    """
    Facade that simplifies video conversion.
    
    Hides the complexity of codec extraction, 
    bitrate reading, audio mixing, and encoding.
    """
    
    def convert(self, filename: str, format: str) -> str:
        """
        Simple interface for video conversion.
        
        Behind the scenes, coordinates many subsystem operations.
        """
        print(f"Converting {filename} to {format}...")
        
        # Complex subsystem interactions
        file = VideoFile(filename)
        codec = CodecFactory.extract(file)
        bitrate = BitrateReader.read(file)
        audio = AudioMixer.fix(codec)
        result = VideoEncoder.encode(file, format, bitrate, audio)
        
        print("  Steps performed internally:")
        print(f"    1. {codec}")
        print(f"    2. Read bitrate: {bitrate}kbps")
        print(f"    3. {audio}")
        print(f"    4. {result}")
        
        return f"output.{format}"


# ============== ANOTHER EXAMPLE: COMPUTER FACADE ==============

class CPU:
    def freeze(self): return "CPU frozen"
    def jump(self, addr): return f"CPU jumped to {addr}"
    def execute(self): return "CPU executing"

class Memory:
    def load(self, addr, data): return f"Memory loaded {data} at {addr}"

class HardDrive:
    def read(self, sector, size): return f"Read {size}b from sector {sector}"

class ComputerFacade:
    """Simplified computer startup."""
    
    def __init__(self):
        self.cpu = CPU()
        self.memory = Memory()
        self.hdd = HardDrive()
    
    def start(self):
        """One simple method to start complex system."""
        steps = [
            self.cpu.freeze(),
            self.hdd.read(0, 1024),
            self.memory.load(0, "BOOT"),
            self.cpu.jump(0),
            self.cpu.execute()
        ]
        return "Computer started: " + " -> ".join(steps)


# ============== USAGE ==============
if __name__ == "__main__":
    print("=== Video Converter Facade ===")
    converter = VideoConverter()
    output = converter.convert("birthday.avi", "mp4")
    print(f"Output: {output}")
    
    print("\n=== Computer Facade ===")
    computer = ComputerFacade()
    print(computer.start())
```

---

## 5. When to Use / Avoid

**Use When:**
- Need simple interface to complex subsystem
- Many dependencies between client and subsystem
- Want to layer your subsystems

**Avoid When:**
- Subsystem is already simple
- Clients need fine-grained control

---

## 6. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Adapter** | Makes incompatible work together; Facade simplifies |
| **Mediator** | Coordinates peers; Facade simplifies one-way calls |

