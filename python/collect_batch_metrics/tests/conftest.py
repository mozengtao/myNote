import json
from pathlib import Path
from typing import Any

import pytest

FIXTURES_DIR = Path(__file__).parent / "fixtures"


@pytest.fixture
def load_json_fixture():
    def _load(name: str) -> dict[str, Any]:
        return json.loads((FIXTURES_DIR / name).read_text(encoding="utf-8"))

    return _load
