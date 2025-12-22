# Builder Pattern in Python

---

## 1. Pattern Name: Builder

**Purpose / Problem Solved:**
Separate the construction of a complex object from its representation, allowing the same construction process to create different representations. Useful for objects with many optional parameters or step-by-step construction.

---

## 2. ASCII Diagram

```
+------------------------------------------------------------------+
|                         CLIENT CODE                               |
|------------------------------------------------------------------|
| director = Director(builder)                                      |
| director.construct()                                              |
| product = builder.get_result()                                    |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                          DIRECTOR                                 |
|------------------------------------------------------------------|
| - builder: Builder                                                |
|------------------------------------------------------------------|
| + construct()                                                     |
|     builder.build_part_a()                                        |
|     builder.build_part_b()                                        |
|     builder.build_part_c()                                        |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                      BUILDER (Abstract)                           |
|------------------------------------------------------------------|
| + build_part_a()                                                  |
| + build_part_b()                                                  |
| + build_part_c()                                                  |
| + get_result() -> Product                                         |
+------------------------------------------------------------------+
         ^                                    ^
         |                                    |
+---------------------+             +---------------------+
|  ConcreteBuilder1   |             |  ConcreteBuilder2   |
|---------------------|             |---------------------|
| - product: Product1 |             | - product: Product2 |
|---------------------|             |---------------------|
| build_part_a()      |             | build_part_a()      |
| build_part_b()      |             | build_part_b()      |
| build_part_c()      |             | build_part_c()      |
| get_result()        |             | get_result()        |
+---------------------+             +---------------------+
         |                                    |
         v                                    v
+---------------------+             +---------------------+
|     Product1        |             |     Product2        |
+---------------------+             +---------------------+

FLUENT BUILDER (Method Chaining):
+------------------------------------------------------------------+
| builder.set_name("X").set_size(10).set_color("red").build()       |
|           |              |              |              |          |
|           v              v              v              v          |
|       returns        returns        returns        returns        |
|        self           self           self          Product        |
+------------------------------------------------------------------+
```

**中文说明：**
建造者模式将复杂对象的构建过程与其表示分离。Director定义构建步骤的顺序，Builder提供具体的构建实现。这允许使用相同的构建过程创建不同的产品表示。Python中常用**流式接口**（Fluent Interface）实现，通过方法链式调用（每个方法返回self）使代码更加优雅。适用于：参数很多、部分可选、需要验证的对象创建场景。

---

## 3. Python Grammar & Tips

| Python Feature | Why It Works for Builder |
|----------------|-------------------------|
| **Method chaining** | Return `self` to enable fluent interface |
| **`@dataclass`** | Auto-generate `__init__` for simple products |
| **`**kwargs`** | Collect arbitrary keyword arguments |
| **Default parameters** | Handle optional fields elegantly |
| **`@property`** | Validate or compute values on access |
| **Named arguments** | Python's named args reduce need for Builder |
| **`__call__`** | Make builder callable like a factory |
| **Context manager** | Use `with` for scoped building |
| **Type hints** | Document expected types for each step |

---

## 4. Typical Example / Project

| Project | Usage |
|---------|-------|
| **SQLAlchemy Query** | `session.query(User).filter(...).order_by(...).limit(10)` |
| **Requests** | `requests.Request().prepare()` builds prepared requests |
| **pytest fixtures** | `@pytest.fixture` with parameters builds test context |
| **argparse** | `ArgumentParser().add_argument()` builds CLI parser |
| **attrs/pydantic** | Builders for complex model validation |
| **Django ORM** | `QuerySet` methods chain to build queries |

```python
# SQLAlchemy Query Builder example
from sqlalchemy.orm import Session

query = (
    session.query(User)
    .filter(User.active == True)
    .filter(User.age >= 18)
    .order_by(User.name)
    .limit(100)
)
# Each method returns a new Query object (immutable builder)
```

---

## 5. Python Module Examples

### Example 1: Classic Builder with Director

```python
#!/usr/bin/env python3
"""
Builder Pattern - Classic Implementation with Director

Separates construction algorithm (Director) from 
construction steps (Builder).
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import List, Optional


# ============== PRODUCT ==============

@dataclass
class House:
    """The complex product being built."""
    foundation: str = ""
    structure: str = ""
    roof: str = ""
    interior: str = ""
    garden: bool = False
    pool: bool = False
    garage: bool = False
    
    def __str__(self):
        parts = [
            f"Foundation: {self.foundation}",
            f"Structure: {self.structure}",
            f"Roof: {self.roof}",
            f"Interior: {self.interior}",
        ]
        if self.garden:
            parts.append("Garden: Yes")
        if self.pool:
            parts.append("Pool: Yes")
        if self.garage:
            parts.append("Garage: Yes")
        return "\n".join(parts)


# ============== BUILDER INTERFACE ==============

class HouseBuilder(ABC):
    """Abstract Builder interface."""
    
    @abstractmethod
    def reset(self) -> None:
        """Reset the builder to start fresh."""
        pass
    
    @abstractmethod
    def build_foundation(self) -> None:
        pass
    
    @abstractmethod
    def build_structure(self) -> None:
        pass
    
    @abstractmethod
    def build_roof(self) -> None:
        pass
    
    @abstractmethod
    def build_interior(self) -> None:
        pass
    
    @abstractmethod
    def build_garden(self) -> None:
        pass
    
    @abstractmethod
    def build_pool(self) -> None:
        pass
    
    @abstractmethod
    def build_garage(self) -> None:
        pass
    
    @abstractmethod
    def get_result(self) -> House:
        """Return the constructed product."""
        pass


# ============== CONCRETE BUILDERS ==============

class WoodenHouseBuilder(HouseBuilder):
    """Builder for wooden houses."""
    
    def __init__(self):
        self._house = House()
    
    def reset(self) -> None:
        self._house = House()
    
    def build_foundation(self) -> None:
        self._house.foundation = "Wooden piles"
    
    def build_structure(self) -> None:
        self._house.structure = "Wooden frame with timber walls"
    
    def build_roof(self) -> None:
        self._house.roof = "Wooden shingles"
    
    def build_interior(self) -> None:
        self._house.interior = "Oak wood paneling"
    
    def build_garden(self) -> None:
        self._house.garden = True
    
    def build_pool(self) -> None:
        # Wooden houses typically don't have pools
        self._house.pool = False
    
    def build_garage(self) -> None:
        self._house.garage = True
    
    def get_result(self) -> House:
        result = self._house
        self.reset()
        return result


class StoneHouseBuilder(HouseBuilder):
    """Builder for stone houses."""
    
    def __init__(self):
        self._house = House()
    
    def reset(self) -> None:
        self._house = House()
    
    def build_foundation(self) -> None:
        self._house.foundation = "Concrete slab with stone base"
    
    def build_structure(self) -> None:
        self._house.structure = "Stone walls with steel reinforcement"
    
    def build_roof(self) -> None:
        self._house.roof = "Slate tiles"
    
    def build_interior(self) -> None:
        self._house.interior = "Plastered walls with marble floors"
    
    def build_garden(self) -> None:
        self._house.garden = True
    
    def build_pool(self) -> None:
        self._house.pool = True
    
    def build_garage(self) -> None:
        self._house.garage = True
    
    def get_result(self) -> House:
        result = self._house
        self.reset()
        return result


class ModernHouseBuilder(HouseBuilder):
    """Builder for modern minimalist houses."""
    
    def __init__(self):
        self._house = House()
    
    def reset(self) -> None:
        self._house = House()
    
    def build_foundation(self) -> None:
        self._house.foundation = "Reinforced concrete foundation"
    
    def build_structure(self) -> None:
        self._house.structure = "Steel frame with glass panels"
    
    def build_roof(self) -> None:
        self._house.roof = "Flat roof with solar panels"
    
    def build_interior(self) -> None:
        self._house.interior = "Open plan with smart home integration"
    
    def build_garden(self) -> None:
        self._house.garden = True
    
    def build_pool(self) -> None:
        self._house.pool = True
    
    def build_garage(self) -> None:
        self._house.garage = True
    
    def get_result(self) -> House:
        result = self._house
        self.reset()
        return result


# ============== DIRECTOR ==============

class ConstructionDirector:
    """
    Director that defines construction sequences.
    
    The director knows WHAT to build, but not HOW.
    The builder knows HOW, but not WHAT sequence.
    """
    
    def __init__(self):
        self._builder: Optional[HouseBuilder] = None
    
    @property
    def builder(self) -> HouseBuilder:
        return self._builder
    
    @builder.setter
    def builder(self, builder: HouseBuilder) -> None:
        self._builder = builder
    
    def build_minimal_house(self) -> None:
        """Build just the basics."""
        self._builder.build_foundation()
        self._builder.build_structure()
        self._builder.build_roof()
    
    def build_standard_house(self) -> None:
        """Build a standard house with interior."""
        self.build_minimal_house()
        self._builder.build_interior()
        self._builder.build_garage()
    
    def build_luxury_house(self) -> None:
        """Build a luxury house with all features."""
        self.build_standard_house()
        self._builder.build_garden()
        self._builder.build_pool()


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Builder Pattern - Classic Implementation")
    print("=" * 60)
    
    director = ConstructionDirector()
    
    # Build different types of houses
    builders = [
        ("Wooden", WoodenHouseBuilder()),
        ("Stone", StoneHouseBuilder()),
        ("Modern", ModernHouseBuilder()),
    ]
    
    for name, builder in builders:
        director.builder = builder
        
        print(f"\n--- {name} House (Luxury) ---")
        director.build_luxury_house()
        house = builder.get_result()
        print(house)
    
    # Build minimal house
    print("\n--- Minimal Wooden House ---")
    director.builder = WoodenHouseBuilder()
    director.build_minimal_house()
    house = director.builder.get_result()
    print(house)
    
    # Build without director (direct control)
    print("\n--- Custom Build (No Director) ---")
    builder = ModernHouseBuilder()
    builder.build_foundation()
    builder.build_structure()
    builder.build_roof()
    builder.build_pool()  # Just the pool, no garden
    house = builder.get_result()
    print(house)
```

---

### Example 2: Fluent Builder (Method Chaining)

```python
#!/usr/bin/env python3
"""
Builder Pattern - Fluent Interface (Method Chaining)

The Pythonic way to implement builders.
Each method returns self to enable chaining.
"""

from dataclasses import dataclass, field
from typing import List, Optional, Self
from enum import Enum, auto


class HttpMethod(Enum):
    GET = auto()
    POST = auto()
    PUT = auto()
    DELETE = auto()
    PATCH = auto()


@dataclass
class HttpRequest:
    """The product: an HTTP request."""
    method: HttpMethod
    url: str
    headers: dict = field(default_factory=dict)
    params: dict = field(default_factory=dict)
    body: Optional[str] = None
    timeout: int = 30
    
    def __str__(self):
        lines = [
            f"{self.method.name} {self.url}",
            f"Headers: {self.headers}",
            f"Params: {self.params}",
            f"Timeout: {self.timeout}s",
        ]
        if self.body:
            lines.append(f"Body: {self.body[:50]}...")
        return "\n".join(lines)


class RequestBuilder:
    """
    Fluent builder for HTTP requests.
    
    Each method returns self for chaining.
    Call build() to get the final product.
    """
    
    def __init__(self, method: HttpMethod, url: str):
        self._method = method
        self._url = url
        self._headers: dict = {}
        self._params: dict = {}
        self._body: Optional[str] = None
        self._timeout: int = 30
    
    def header(self, key: str, value: str) -> Self:
        """Add a header."""
        self._headers[key] = value
        return self
    
    def headers(self, headers: dict) -> Self:
        """Add multiple headers."""
        self._headers.update(headers)
        return self
    
    def param(self, key: str, value: str) -> Self:
        """Add a query parameter."""
        self._params[key] = value
        return self
    
    def params(self, params: dict) -> Self:
        """Add multiple query parameters."""
        self._params.update(params)
        return self
    
    def body(self, content: str) -> Self:
        """Set the request body."""
        self._body = content
        return self
    
    def json(self, data: dict) -> Self:
        """Set JSON body and content-type header."""
        import json
        self._body = json.dumps(data)
        self._headers["Content-Type"] = "application/json"
        return self
    
    def timeout(self, seconds: int) -> Self:
        """Set request timeout."""
        self._timeout = seconds
        return self
    
    def auth(self, token: str) -> Self:
        """Add authorization header."""
        self._headers["Authorization"] = f"Bearer {token}"
        return self
    
    def build(self) -> HttpRequest:
        """Build and return the HTTP request."""
        return HttpRequest(
            method=self._method,
            url=self._url,
            headers=self._headers,
            params=self._params,
            body=self._body,
            timeout=self._timeout,
        )
    
    # Class methods for common requests
    @classmethod
    def get(cls, url: str) -> "RequestBuilder":
        """Start building a GET request."""
        return cls(HttpMethod.GET, url)
    
    @classmethod
    def post(cls, url: str) -> "RequestBuilder":
        """Start building a POST request."""
        return cls(HttpMethod.POST, url)
    
    @classmethod
    def put(cls, url: str) -> "RequestBuilder":
        """Start building a PUT request."""
        return cls(HttpMethod.PUT, url)
    
    @classmethod
    def delete(cls, url: str) -> "RequestBuilder":
        """Start building a DELETE request."""
        return cls(HttpMethod.DELETE, url)


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Builder Pattern - Fluent Interface")
    print("=" * 60)
    
    # Simple GET request
    print("\n--- Simple GET ---")
    request = (
        RequestBuilder.get("https://api.example.com/users")
        .build()
    )
    print(request)
    
    # GET with parameters and headers
    print("\n--- GET with params and headers ---")
    request = (
        RequestBuilder.get("https://api.example.com/users")
        .param("page", "1")
        .param("limit", "10")
        .header("Accept", "application/json")
        .timeout(60)
        .build()
    )
    print(request)
    
    # POST with JSON body
    print("\n--- POST with JSON ---")
    request = (
        RequestBuilder.post("https://api.example.com/users")
        .json({
            "name": "Alice",
            "email": "alice@example.com",
            "role": "admin"
        })
        .auth("secret-token-123")
        .timeout(10)
        .build()
    )
    print(request)
    
    # Complex request
    print("\n--- Complex PUT request ---")
    request = (
        RequestBuilder.put("https://api.example.com/users/123")
        .headers({
            "Accept": "application/json",
            "X-Request-ID": "abc-123",
        })
        .auth("token-456")
        .json({"status": "active"})
        .timeout(30)
        .build()
    )
    print(request)
```

---

### Example 3: Step Builder (Type-Safe)

```python
#!/usr/bin/env python3
"""
Builder Pattern - Step Builder (Guided Construction)

Uses type system to enforce construction order.
Each step returns a new builder type with specific methods available.
"""

from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class Email:
    """The product: an email message."""
    sender: str
    recipient: str
    subject: str
    body: str
    cc: tuple[str, ...] = ()
    bcc: tuple[str, ...] = ()
    attachments: tuple[str, ...] = ()
    
    def __str__(self):
        lines = [
            f"From: {self.sender}",
            f"To: {self.recipient}",
        ]
        if self.cc:
            lines.append(f"CC: {', '.join(self.cc)}")
        if self.bcc:
            lines.append(f"BCC: {', '.join(self.bcc)}")
        lines.extend([
            f"Subject: {self.subject}",
            "-" * 40,
            self.body,
        ])
        if self.attachments:
            lines.append(f"\nAttachments: {', '.join(self.attachments)}")
        return "\n".join(lines)


# ============== STEP BUILDER CLASSES ==============

class EmailBuilderStart:
    """Step 1: Must specify sender."""
    
    def from_address(self, sender: str) -> "EmailBuilderWithSender":
        """Set the sender address."""
        return EmailBuilderWithSender(sender)


class EmailBuilderWithSender:
    """Step 2: Must specify recipient."""
    
    def __init__(self, sender: str):
        self._sender = sender
    
    def to_address(self, recipient: str) -> "EmailBuilderWithRecipient":
        """Set the recipient address."""
        return EmailBuilderWithRecipient(self._sender, recipient)


class EmailBuilderWithRecipient:
    """Step 3: Must specify subject."""
    
    def __init__(self, sender: str, recipient: str):
        self._sender = sender
        self._recipient = recipient
    
    def subject(self, subject: str) -> "EmailBuilderWithSubject":
        """Set the subject."""
        return EmailBuilderWithSubject(
            self._sender, self._recipient, subject
        )


class EmailBuilderWithSubject:
    """Step 4: Must specify body."""
    
    def __init__(self, sender: str, recipient: str, subject: str):
        self._sender = sender
        self._recipient = recipient
        self._subject = subject
    
    def body(self, body: str) -> "EmailBuilderComplete":
        """Set the body content."""
        return EmailBuilderComplete(
            self._sender, self._recipient, self._subject, body
        )


class EmailBuilderComplete:
    """
    Final step: All required fields set.
    Now optional fields and build() are available.
    """
    
    def __init__(self, sender: str, recipient: str, subject: str, body: str):
        self._sender = sender
        self._recipient = recipient
        self._subject = subject
        self._body = body
        self._cc: list[str] = []
        self._bcc: list[str] = []
        self._attachments: list[str] = []
    
    def cc(self, *addresses: str) -> "EmailBuilderComplete":
        """Add CC recipients."""
        self._cc.extend(addresses)
        return self
    
    def bcc(self, *addresses: str) -> "EmailBuilderComplete":
        """Add BCC recipients."""
        self._bcc.extend(addresses)
        return self
    
    def attach(self, *files: str) -> "EmailBuilderComplete":
        """Add attachments."""
        self._attachments.extend(files)
        return self
    
    def build(self) -> Email:
        """Build and return the email."""
        return Email(
            sender=self._sender,
            recipient=self._recipient,
            subject=self._subject,
            body=self._body,
            cc=tuple(self._cc),
            bcc=tuple(self._bcc),
            attachments=tuple(self._attachments),
        )


# ============== CONVENIENCE FUNCTION ==============

def email() -> EmailBuilderStart:
    """Start building an email."""
    return EmailBuilderStart()


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Builder Pattern - Step Builder (Type-Safe)")
    print("=" * 60)
    
    # The order is enforced by the type system!
    # Can't call .subject() before .to_address()
    
    # Minimal email
    print("\n--- Minimal Email ---")
    msg = (
        email()
        .from_address("alice@example.com")
        .to_address("bob@example.com")
        .subject("Meeting Tomorrow")
        .body("Hi Bob,\n\nLet's meet at 3pm.\n\nAlice")
        .build()
    )
    print(msg)
    
    # Full email with all options
    print("\n--- Full Email ---")
    msg = (
        email()
        .from_address("ceo@company.com")
        .to_address("all@company.com")
        .subject("Q4 Results")
        .body("Dear Team,\n\nPlease find the Q4 results attached.\n\nBest regards")
        .cc("board@company.com", "investors@company.com")
        .bcc("legal@company.com")
        .attach("q4_report.pdf", "financials.xlsx")
        .build()
    )
    print(msg)
    
    # The following would cause type errors in IDE/mypy:
    # email().subject("Test")  # Error: no .subject() on EmailBuilderStart
    # email().from_address("a@b.c").body("text")  # Error: no .body() yet
```

---

### Example 4: Configuration Builder with Validation

```python
#!/usr/bin/env python3
"""
Builder Pattern - Configuration with Validation

Builds configuration objects with validation at build time.
Demonstrates defensive building with clear error messages.
"""

from dataclasses import dataclass, field
from typing import List, Optional, Self
from enum import Enum
import re


class Environment(Enum):
    DEVELOPMENT = "development"
    STAGING = "staging"
    PRODUCTION = "production"


class ConfigurationError(Exception):
    """Raised when configuration validation fails."""
    pass


@dataclass(frozen=True)
class ServerConfig:
    """Immutable server configuration."""
    host: str
    port: int
    environment: Environment
    workers: int
    debug: bool
    database_url: str
    secret_key: str
    allowed_hosts: tuple[str, ...]
    cors_origins: tuple[str, ...]
    log_level: str


class ServerConfigBuilder:
    """
    Builder with validation.
    
    Validates configuration at build time and provides
    helpful error messages.
    """
    
    def __init__(self):
        self._host: Optional[str] = None
        self._port: Optional[int] = None
        self._environment: Environment = Environment.DEVELOPMENT
        self._workers: int = 1
        self._debug: Optional[bool] = None
        self._database_url: Optional[str] = None
        self._secret_key: Optional[str] = None
        self._allowed_hosts: List[str] = []
        self._cors_origins: List[str] = []
        self._log_level: str = "INFO"
        self._errors: List[str] = []
    
    def host(self, host: str) -> Self:
        """Set server host."""
        if not host:
            self._errors.append("Host cannot be empty")
        self._host = host
        return self
    
    def port(self, port: int) -> Self:
        """Set server port (1-65535)."""
        if not 1 <= port <= 65535:
            self._errors.append(f"Port must be between 1 and 65535, got {port}")
        self._port = port
        return self
    
    def environment(self, env: Environment) -> Self:
        """Set environment."""
        self._environment = env
        return self
    
    def workers(self, count: int) -> Self:
        """Set number of workers (1-32)."""
        if not 1 <= count <= 32:
            self._errors.append(f"Workers must be between 1 and 32, got {count}")
        self._workers = count
        return self
    
    def debug(self, enabled: bool) -> Self:
        """Enable/disable debug mode."""
        self._debug = enabled
        return self
    
    def database_url(self, url: str) -> Self:
        """Set database connection URL."""
        pattern = r"^(postgres|mysql|sqlite)://"
        if not re.match(pattern, url):
            self._errors.append(f"Invalid database URL format: {url}")
        self._database_url = url
        return self
    
    def secret_key(self, key: str) -> Self:
        """Set secret key (min 32 characters for production)."""
        self._secret_key = key
        return self
    
    def allowed_hosts(self, *hosts: str) -> Self:
        """Set allowed hosts."""
        self._allowed_hosts.extend(hosts)
        return self
    
    def cors_origins(self, *origins: str) -> Self:
        """Set CORS allowed origins."""
        self._cors_origins.extend(origins)
        return self
    
    def log_level(self, level: str) -> Self:
        """Set log level."""
        valid_levels = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"}
        if level.upper() not in valid_levels:
            self._errors.append(f"Invalid log level: {level}")
        self._log_level = level.upper()
        return self
    
    def _validate(self) -> List[str]:
        """Validate the configuration and return errors."""
        errors = self._errors.copy()
        
        # Required fields
        if not self._host:
            errors.append("Host is required")
        if not self._port:
            errors.append("Port is required")
        if not self._database_url:
            errors.append("Database URL is required")
        if not self._secret_key:
            errors.append("Secret key is required")
        
        # Production-specific validation
        if self._environment == Environment.PRODUCTION:
            if self._debug:
                errors.append("Debug mode must be disabled in production")
            if self._secret_key and len(self._secret_key) < 32:
                errors.append("Secret key must be at least 32 characters in production")
            if not self._allowed_hosts:
                errors.append("Allowed hosts must be specified in production")
        
        return errors
    
    def build(self) -> ServerConfig:
        """
        Build and validate the configuration.
        
        Raises:
            ConfigurationError: If validation fails
        """
        # Set defaults based on environment
        if self._debug is None:
            self._debug = self._environment == Environment.DEVELOPMENT
        
        # Validate
        errors = self._validate()
        if errors:
            error_list = "\n  - ".join(errors)
            raise ConfigurationError(f"Configuration errors:\n  - {error_list}")
        
        return ServerConfig(
            host=self._host,  # type: ignore
            port=self._port,  # type: ignore
            environment=self._environment,
            workers=self._workers,
            debug=self._debug,
            database_url=self._database_url,  # type: ignore
            secret_key=self._secret_key,  # type: ignore
            allowed_hosts=tuple(self._allowed_hosts),
            cors_origins=tuple(self._cors_origins),
            log_level=self._log_level,
        )
    
    # Preset configurations
    @classmethod
    def development(cls) -> "ServerConfigBuilder":
        """Start with development defaults."""
        return (
            cls()
            .host("127.0.0.1")
            .port(8000)
            .environment(Environment.DEVELOPMENT)
            .debug(True)
            .workers(1)
            .log_level("DEBUG")
        )
    
    @classmethod
    def production(cls) -> "ServerConfigBuilder":
        """Start with production defaults."""
        return (
            cls()
            .host("0.0.0.0")
            .port(8080)
            .environment(Environment.PRODUCTION)
            .debug(False)
            .workers(4)
            .log_level("WARNING")
        )


# ============== USAGE DEMO ==============
if __name__ == "__main__":
    print("=" * 60)
    print("Builder Pattern - Configuration with Validation")
    print("=" * 60)
    
    # Development config
    print("\n--- Development Configuration ---")
    try:
        config = (
            ServerConfigBuilder.development()
            .database_url("sqlite:///dev.db")
            .secret_key("dev-secret-key")
            .build()
        )
        print(f"Host: {config.host}:{config.port}")
        print(f"Environment: {config.environment.value}")
        print(f"Debug: {config.debug}")
        print(f"Workers: {config.workers}")
    except ConfigurationError as e:
        print(f"Error: {e}")
    
    # Production config
    print("\n--- Production Configuration ---")
    try:
        config = (
            ServerConfigBuilder.production()
            .database_url("postgres://prod:5432/myapp")
            .secret_key("a" * 64)  # Long enough for production
            .allowed_hosts("myapp.com", "www.myapp.com")
            .cors_origins("https://myapp.com")
            .build()
        )
        print(f"Host: {config.host}:{config.port}")
        print(f"Environment: {config.environment.value}")
        print(f"Allowed hosts: {config.allowed_hosts}")
    except ConfigurationError as e:
        print(f"Error: {e}")
    
    # Invalid config (shows validation)
    print("\n--- Invalid Configuration (Validation Demo) ---")
    try:
        config = (
            ServerConfigBuilder()
            .environment(Environment.PRODUCTION)
            .debug(True)  # Error: debug in production
            .secret_key("short")  # Error: too short
            # Missing: host, port, database_url
            .build()
        )
    except ConfigurationError as e:
        print(f"{e}")
```

---

## 6. When to Use / When to Avoid

### Use When:
- Object has **many parameters** (especially optional ones)
- Construction involves **multiple steps**
- You need **different representations** from the same process
- You want to **validate** the final object before creation
- Object should be **immutable** after construction

### Avoid When:
- Object is simple with few required parameters
- Python's named arguments suffice
- No complex construction logic needed

### Python Alternative:
Often you can use **dataclasses with defaults** or **named parameters**:

```python
# Instead of Builder, sometimes this is enough:
@dataclass
class Config:
    host: str = "localhost"
    port: int = 8080
    debug: bool = False
    workers: int = 4

# Simple construction with named args
config = Config(host="0.0.0.0", workers=8)
```

---

## 7. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Abstract Factory** | Builder focuses on step-by-step; Factory on families of objects |
| **Composite** | Builder often used to create Composite trees |
| **Prototype** | Alternative - clone and modify instead of build step-by-step |
| **Fluent Interface** | Common implementation technique for Builder |

