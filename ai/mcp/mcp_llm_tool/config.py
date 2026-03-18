"""
Configuration management for the MCP LLM Tool.

Centralizes all runtime settings: LLM backend selection, API keys,
server addresses, timeouts, and export paths. Values are read from
environment variables with sensible defaults for local development.
"""

import os
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


class LLMBackend(Enum):
    OPENAI = "openai"
    LOCAL = "local"


@dataclass
class OpenAIConfig:
    api_key: str = ""
    model: str = "gpt-4o-mini"
    temperature: float = 0.3
    max_tokens: int = 1024

    def __post_init__(self):
        self.api_key = self.api_key or os.getenv("OPENAI_API_KEY", "")


@dataclass
class LocalLLMConfig:
    base_url: str = "http://localhost:8080/v1"
    model: str = "llama3"
    temperature: float = 0.3
    max_tokens: int = 1024

    def __post_init__(self):
        self.base_url = self.base_url or os.getenv(
            "LOCAL_LLM_URL", "http://localhost:8080/v1"
        )
        self.model = self.model or os.getenv("LOCAL_LLM_MODEL", "llama3")


@dataclass
class ServerConfig:
    host: str = "0.0.0.0"
    port: int = 8000
    reload: bool = False
    log_level: str = "info"


@dataclass
class AppConfig:
    llm_backend: LLMBackend = LLMBackend.OPENAI
    openai: OpenAIConfig = field(default_factory=OpenAIConfig)
    local_llm: LocalLLMConfig = field(default_factory=LocalLLMConfig)
    server: ServerConfig = field(default_factory=ServerConfig)

    request_timeout: float = 30.0
    max_retries: int = 3
    csv_export_dir: str = "./exports"

    def __post_init__(self):
        backend_env = os.getenv("LLM_BACKEND", "openai").lower()
        if backend_env == "local":
            self.llm_backend = LLMBackend.LOCAL

        self.csv_export_dir = os.getenv("CSV_EXPORT_DIR", self.csv_export_dir)
        os.makedirs(self.csv_export_dir, exist_ok=True)


def load_config() -> AppConfig:
    """Build application config from environment variables."""
    return AppConfig()
