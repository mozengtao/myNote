# ofdma-monitor (Python)

`monitor_csl_ofdma_mer.sh` / `monitor_csl_ofdma_uncorrectable.sh` / `run_ofdma_monitors.sh`
（以及 `evccli.sh` / `vmccli.sh`）的 Python 重新实现。原有 bash 脚本保持不变，本目录是一个完全独立的
实现，不依赖也不修改仓库根目录下的任何 shell 脚本。

详细架构、目录结构、扩展指南见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 快速开始

```bash
cd python3
python3 -m venv .venv && source .venv/bin/activate
pip install -e ".[test]"

python3 -m ofdma_monitor run \
  --metrics mer,uncorrectable \
  --output-dir ../output \
  --interval 600 \
  --max-parallel 12 \
  --top-n 100
```

## 排查/验证单条 CLI 命令

在依赖真实 `nomad`/`ncs_cli`/`confd_cli` 的环境里跑 `run` 之前（或遇到 VMC/CM 数量为 0
等异常时），可以用 `debug-evc` / `debug-vmc` 单独发一条命令，同时看到原始 CLI 输出和
`json_utils.extract_json_object` 解析出来的 JSON，方便确认命令语法、噪声剥离、JSON
结构是否符合 `docs/ARCHITECTURE.md` 里记录的假设：

```bash
# 等价于 ./evccli.sh 'show vmc status | display json'
python3 -m ofdma_monitor debug-evc "show vmc status | display json"

# 等价于 ./vmccli.sh "<job>" "<command>"
python3 -m ofdma_monitor debug-vmc vmc-morris-dentist-1 \
  "show ccap docsis docs-mac-domain mac-domain modem brief | display json"
```

## 运行测试

```bash
cd python
pytest
```
