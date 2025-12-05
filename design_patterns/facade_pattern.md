# Facade Pattern (外观模式)

## ASCII Diagram

```
Without Facade:                          With Facade:

+--------+                               +--------+
| Client |                               | Client |
+---+----+                               +---+----+
    |                                        |
    |  (complex dependencies)                | (simple interface)
    |                                        v
    +----+----+----+----+              +------------+
    |    |    |    |    |              |   Facade   |
    v    v    v    v    v              +-----+------+
+---+ +---+ +---+ +---+ +---+                |
|S1 | |S2 | |S3 | |S4 | |S5 |                +----+----+----+----+
+---+ +---+ +---+ +---+ +---+                |    |    |    |    |
  Subsystem Classes                          v    v    v    v    v
                                        +---+ +---+ +---+ +---+ +---+
                                        |S1 | |S2 | |S3 | |S4 | |S5 |
                                        +---+ +---+ +---+ +---+ +---+
                                          Subsystem Classes

Facade Structure:
+-------------------+
|      Facade       |
+-------------------+
| - subsystem1      |-----> [Subsystem1]
| - subsystem2      |-----> [Subsystem2]
| - subsystem3      |-----> [Subsystem3]
+-------------------+
| + simpleOperation1|
| + simpleOperation2|
+-------------------+
```

**中文说明：**
- **Facade（外观）**：为复杂子系统提供简单统一的接口
- **Subsystem（子系统）**：实现具体功能的类，不知道 Facade 的存在
- **Client（客户端）**：通过 Facade 访问子系统
- **关键点**：简化接口，降低客户端与子系统的耦合

---

## 核心思想

为子系统中的一组接口提供一个**统一的高层接口**。外观模式定义了一个高层接口，让子系统更容易使用。它不是封装子系统，而是提供一个简化的入口。

---

## 应用场景

1. **简化复杂接口**：为复杂子系统提供简单接口
2. **层次化结构**：构建多层系统时，用外观定义每层的入口
3. **减少依赖**：客户端只依赖外观，不直接依赖子系统
4. **实际应用**：
   - 视频转换工具（封装编解码器、格式转换、压缩等）
   - 编译器（封装词法分析、语法分析、代码生成等）
   - 家庭影院系统（封装电视、音响、灯光等控制）
   - 电商下单流程（封装库存、支付、物流等）

---

## 优缺点

### 优点
| 优点 | 说明 |
|------|------|
| 简化接口 | 客户端只需了解外观接口 |
| 减少耦合 | 客户端与子系统解耦 |
| 更好的层次 | 实现子系统的分层 |
| 易于使用 | 提供默认的使用方式 |

### 缺点
| 缺点 | 说明 |
|------|------|
| 可能成为万能类 | 外观可能承担太多职责 |
| 不符合开闭原则 | 新增功能可能需要修改外观 |
| 限制灵活性 | 过度简化可能限制高级用法 |

---

## Python 代码示例

### 应用前：直接操作复杂子系统

```python
# 问题：视频转换涉及多个复杂的子系统

class VideoFile:
    """视频文件"""
    def __init__(self, filename):
        self.filename = filename
        self.codec = self._detect_codec()
    
    def _detect_codec(self):
        if self.filename.endswith('.mp4'):
            return 'h264'
        elif self.filename.endswith('.avi'):
            return 'mpeg4'
        return 'unknown'


class CodecFactory:
    """编解码器工厂"""
    @staticmethod
    def extract_audio(video_file):
        print(f"Extracting audio from {video_file.filename}")
        return f"audio_from_{video_file.filename}"
    
    @staticmethod
    def extract_video(video_file):
        print(f"Extracting video stream from {video_file.filename}")
        return f"video_from_{video_file.filename}"


class MPEG4Codec:
    """MPEG4 编解码器"""
    def decode(self, data):
        print(f"Decoding MPEG4: {data}")
        return f"decoded_{data}"
    
    def encode(self, data):
        print(f"Encoding to MPEG4: {data}")
        return f"mpeg4_{data}"


class H264Codec:
    """H264 编解码器"""
    def decode(self, data):
        print(f"Decoding H264: {data}")
        return f"decoded_{data}"
    
    def encode(self, data):
        print(f"Encoding to H264: {data}")
        return f"h264_{data}"


class AudioMixer:
    """音频混合器"""
    def fix(self, audio):
        print(f"Fixing audio: {audio}")
        return f"fixed_{audio}"
    
    def normalize(self, audio):
        print(f"Normalizing audio: {audio}")
        return f"normalized_{audio}"


class BitrateReader:
    """比特率读取器"""
    @staticmethod
    def read(filename, codec):
        print(f"Reading bitrate of {filename} with {codec}")
        return 5000  # kbps
    
    @staticmethod
    def convert(buffer, target_bitrate):
        print(f"Converting buffer to {target_bitrate} kbps")
        return f"converted_{buffer}"


class VideoBuffer:
    """视频缓冲区"""
    def __init__(self):
        self.data = []
    
    def add(self, frame):
        self.data.append(frame)


class FileWriter:
    """文件写入器"""
    @staticmethod
    def write(filename, data):
        print(f"Writing to {filename}: {data[:50]}...")
        return True


# 客户端代码 - 直接使用子系统，非常复杂
def convert_video_directly(input_file, output_format):
    """直接调用各个子系统 - 复杂且容易出错"""
    
    # 1. 加载源文件
    video = VideoFile(input_file)
    print(f"Loading video: {video.filename}, codec: {video.codec}")
    
    # 2. 选择解码器
    if video.codec == 'h264':
        decoder = H264Codec()
    else:
        decoder = MPEG4Codec()
    
    # 3. 提取音视频
    audio = CodecFactory.extract_audio(video)
    video_stream = CodecFactory.extract_video(video)
    
    # 4. 解码
    decoded_audio = decoder.decode(audio)
    decoded_video = decoder.decode(video_stream)
    
    # 5. 处理音频
    mixer = AudioMixer()
    fixed_audio = mixer.fix(decoded_audio)
    normalized_audio = mixer.normalize(fixed_audio)
    
    # 6. 处理比特率
    bitrate = BitrateReader.read(input_file, video.codec)
    target_bitrate = min(bitrate, 3000)  # 限制最大比特率
    
    # 7. 选择编码器
    if output_format == 'mp4':
        encoder = H264Codec()
        output_file = input_file.rsplit('.', 1)[0] + '.mp4'
    else:
        encoder = MPEG4Codec()
        output_file = input_file.rsplit('.', 1)[0] + '.avi'
    
    # 8. 编码
    buffer = VideoBuffer()
    encoded_video = encoder.encode(decoded_video)
    encoded_audio = encoder.encode(normalized_audio)
    buffer.add(encoded_video)
    buffer.add(encoded_audio)
    
    # 9. 转换比特率
    converted = BitrateReader.convert(str(buffer.data), target_bitrate)
    
    # 10. 写入文件
    FileWriter.write(output_file, converted)
    
    return output_file


# 使用 - 客户端需要了解所有子系统
print("=== Without Facade ===")
convert_video_directly("movie.avi", "mp4")
```

### 应用后：使用外观模式

```python
from abc import ABC, abstractmethod
from typing import Optional
from dataclasses import dataclass
from enum import Enum


# ========== 子系统类（保持不变）==========
class VideoFile:
    def __init__(self, filename: str):
        self.filename = filename
        self.codec = self._detect_codec()
        self.duration = 0
        self.resolution = (1920, 1080)
    
    def _detect_codec(self) -> str:
        extensions = {'.mp4': 'h264', '.avi': 'mpeg4', '.mkv': 'h265'}
        for ext, codec in extensions.items():
            if self.filename.endswith(ext):
                return codec
        return 'unknown'


class Codec(ABC):
    @abstractmethod
    def decode(self, data: str) -> str:
        pass
    
    @abstractmethod
    def encode(self, data: str) -> str:
        pass


class H264Codec(Codec):
    def decode(self, data: str) -> str:
        print(f"  [H264] Decoding: {data[:30]}...")
        return f"decoded_h264_{data}"
    
    def encode(self, data: str) -> str:
        print(f"  [H264] Encoding: {data[:30]}...")
        return f"encoded_h264_{data}"


class MPEG4Codec(Codec):
    def decode(self, data: str) -> str:
        print(f"  [MPEG4] Decoding: {data[:30]}...")
        return f"decoded_mpeg4_{data}"
    
    def encode(self, data: str) -> str:
        print(f"  [MPEG4] Encoding: {data[:30]}...")
        return f"encoded_mpeg4_{data}"


class H265Codec(Codec):
    def decode(self, data: str) -> str:
        print(f"  [H265] Decoding: {data[:30]}...")
        return f"decoded_h265_{data}"
    
    def encode(self, data: str) -> str:
        print(f"  [H265] Encoding: {data[:30]}...")
        return f"encoded_h265_{data}"


class AudioMixer:
    def extract(self, video: VideoFile) -> str:
        print(f"  [AudioMixer] Extracting audio from {video.filename}")
        return f"audio_{video.filename}"
    
    def fix(self, audio: str) -> str:
        print(f"  [AudioMixer] Fixing audio levels")
        return f"fixed_{audio}"
    
    def normalize(self, audio: str, target_db: float = -14.0) -> str:
        print(f"  [AudioMixer] Normalizing to {target_db}dB")
        return f"normalized_{audio}"


class BitrateReader:
    def read(self, video: VideoFile) -> int:
        print(f"  [BitrateReader] Reading bitrate of {video.filename}")
        return 5000
    
    def convert(self, data: str, target_kbps: int) -> str:
        print(f"  [BitrateReader] Converting to {target_kbps} kbps")
        return f"bitrate_{target_kbps}_{data}"


class FileWriter:
    def write(self, filename: str, data: str) -> bool:
        print(f"  [FileWriter] Writing {len(data)} bytes to {filename}")
        return True
    
    def get_temp_path(self, filename: str) -> str:
        return f"/tmp/{filename}"


class ProgressReporter:
    def __init__(self, total_steps: int):
        self.total = total_steps
        self.current = 0
    
    def update(self, step: str):
        self.current += 1
        percent = (self.current / self.total) * 100
        print(f"  [{percent:5.1f}%] {step}")


# ========== 外观类 ==========
class VideoFormat(Enum):
    MP4 = "mp4"
    AVI = "avi"
    MKV = "mkv"


@dataclass
class ConversionOptions:
    """转换选项"""
    target_format: VideoFormat = VideoFormat.MP4
    max_bitrate: int = 3000  # kbps
    normalize_audio: bool = True
    target_audio_db: float = -14.0
    resolution: Optional[tuple] = None


class VideoConverterFacade:
    """
    视频转换外观类
    
    封装了复杂的视频转换流程，提供简单的接口
    """
    
    def __init__(self):
        # 初始化子系统
        self._codecs = {
            'h264': H264Codec(),
            'mpeg4': MPEG4Codec(),
            'h265': H265Codec(),
        }
        self._format_codecs = {
            VideoFormat.MP4: 'h264',
            VideoFormat.AVI: 'mpeg4',
            VideoFormat.MKV: 'h265',
        }
        self._audio_mixer = AudioMixer()
        self._bitrate_reader = BitrateReader()
        self._file_writer = FileWriter()
    
    def convert(self, input_path: str, 
                options: Optional[ConversionOptions] = None) -> str:
        """
        转换视频到指定格式
        
        这是外观提供的简化接口，封装了所有复杂的子系统交互
        """
        options = options or ConversionOptions()
        progress = ProgressReporter(6)
        
        print(f"\n{'='*50}")
        print(f"Converting: {input_path} -> {options.target_format.value}")
        print('='*50)
        
        # 1. 加载源文件
        progress.update("Loading source file")
        video = VideoFile(input_path)
        
        # 2. 获取解码器
        progress.update("Preparing decoder")
        decoder = self._codecs.get(video.codec, self._codecs['mpeg4'])
        
        # 3. 解码视频和音频
        progress.update("Decoding media")
        decoded_video = decoder.decode(video.filename)
        audio = self._audio_mixer.extract(video)
        decoded_audio = decoder.decode(audio)
        
        # 4. 处理音频
        progress.update("Processing audio")
        fixed_audio = self._audio_mixer.fix(decoded_audio)
        if options.normalize_audio:
            fixed_audio = self._audio_mixer.normalize(
                fixed_audio, options.target_audio_db
            )
        
        # 5. 编码到目标格式
        progress.update("Encoding to target format")
        target_codec_name = self._format_codecs[options.target_format]
        encoder = self._codecs[target_codec_name]
        
        # 处理比特率
        current_bitrate = self._bitrate_reader.read(video)
        target_bitrate = min(current_bitrate, options.max_bitrate)
        
        encoded_video = encoder.encode(decoded_video)
        encoded_video = self._bitrate_reader.convert(encoded_video, target_bitrate)
        encoded_audio = encoder.encode(fixed_audio)
        
        # 6. 写入文件
        progress.update("Writing output file")
        output_path = self._get_output_path(input_path, options.target_format)
        final_data = f"{encoded_video}|{encoded_audio}"
        self._file_writer.write(output_path, final_data)
        
        print(f"\n✓ Conversion complete: {output_path}")
        return output_path
    
    def _get_output_path(self, input_path: str, format: VideoFormat) -> str:
        base = input_path.rsplit('.', 1)[0]
        return f"{base}_converted.{format.value}"
    
    # 提供一些便捷方法
    def convert_to_mp4(self, input_path: str) -> str:
        """快速转换为 MP4"""
        return self.convert(input_path, ConversionOptions(
            target_format=VideoFormat.MP4
        ))
    
    def convert_to_web(self, input_path: str) -> str:
        """转换为网页友好格式（低比特率 MP4）"""
        return self.convert(input_path, ConversionOptions(
            target_format=VideoFormat.MP4,
            max_bitrate=1500,
            normalize_audio=True
        ))
    
    def get_video_info(self, input_path: str) -> dict:
        """获取视频信息（简单查询不需要完整转换）"""
        video = VideoFile(input_path)
        bitrate = self._bitrate_reader.read(video)
        return {
            "filename": video.filename,
            "codec": video.codec,
            "resolution": video.resolution,
            "bitrate_kbps": bitrate
        }


# ========== 使用示例 ==========
if __name__ == "__main__":
    
    # 创建外观
    converter = VideoConverterFacade()
    
    print("\n" + "=" * 60)
    print("1. Simple conversion (default options)")
    print("=" * 60)
    converter.convert("movie.avi")
    
    print("\n" + "=" * 60)
    print("2. Convert with custom options")
    print("=" * 60)
    options = ConversionOptions(
        target_format=VideoFormat.MKV,
        max_bitrate=2000,
        normalize_audio=True,
        target_audio_db=-12.0
    )
    converter.convert("home_video.mp4", options)
    
    print("\n" + "=" * 60)
    print("3. Quick convert to web format")
    print("=" * 60)
    converter.convert_to_web("presentation.avi")
    
    print("\n" + "=" * 60)
    print("4. Get video info")
    print("=" * 60)
    info = converter.get_video_info("sample.mkv")
    print(f"Video info: {info}")


# ========== 高级用法：多个外观 ==========
class HomeTheaterFacade:
    """家庭影院外观"""
    
    def __init__(self):
        self.tv = TV()
        self.sound_system = SoundSystem()
        self.streaming = StreamingPlayer()
        self.lights = SmartLights()
    
    def watch_movie(self, movie: str):
        """一键观影"""
        print("\n🎬 Starting movie mode...")
        self.lights.dim(20)
        self.tv.on()
        self.tv.set_input("HDMI1")
        self.sound_system.on()
        self.sound_system.set_mode("surround")
        self.sound_system.set_volume(50)
        self.streaming.on()
        self.streaming.play(movie)
        print("✓ Enjoy your movie!")
    
    def end_movie(self):
        """一键结束"""
        print("\n🔚 Ending movie mode...")
        self.streaming.off()
        self.sound_system.off()
        self.tv.off()
        self.lights.on()
        print("✓ Movie mode ended")


# 子系统类
class TV:
    def on(self): print("  [TV] Turning on")
    def off(self): print("  [TV] Turning off")
    def set_input(self, input): print(f"  [TV] Setting input to {input}")

class SoundSystem:
    def on(self): print("  [Sound] Turning on")
    def off(self): print("  [Sound] Turning off")
    def set_mode(self, mode): print(f"  [Sound] Setting mode to {mode}")
    def set_volume(self, level): print(f"  [Sound] Setting volume to {level}")

class StreamingPlayer:
    def on(self): print("  [Streaming] Turning on")
    def off(self): print("  [Streaming] Turning off")
    def play(self, title): print(f"  [Streaming] Playing: {title}")

class SmartLights:
    def on(self): print("  [Lights] Turning on (100%)")
    def off(self): print("  [Lights] Turning off")
    def dim(self, level): print(f"  [Lights] Dimming to {level}%")


# 使用家庭影院外观
theater = HomeTheaterFacade()
theater.watch_movie("Inception")
# ... 观影中 ...
theater.end_movie()
```

---

## 应用该设计模式的优势

| 对比项 | 应用前 | 应用后 |
|--------|--------|--------|
| **复杂度** | 客户端需了解所有子系统 | 只需了解外观接口 |
| **耦合度** | 客户端与多个子系统耦合 | 只与外观耦合 |
| **使用难度** | 需要正确的调用顺序 | 简单方法调用 |
| **维护性** | 子系统变化影响所有客户端 | 只需修改外观 |
| **灵活性** | 客户端仍可直接访问子系统 | 保留直接访问的可能 |

---

## 与其他模式的关系

| 模式 | 目的 | 区别 |
|------|------|------|
| **Facade** | 简化接口 | 提供统一入口，不增加功能 |
| **Adapter** | 转换接口 | 让不兼容的接口协同工作 |
| **Mediator** | 协调交互 | 对象间的通信中介 |
| **Singleton** | 唯一实例 | 外观通常是单例 |

