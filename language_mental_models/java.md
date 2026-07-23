# Java —— 职责（Responsibility）

> **核心驱动力：系统由多个对象协同完成各自职责。**
> Java 程序员不问"这段逻辑写在哪个方法里最方便"，而问"这件事到底该由谁负责"。

---

## 心智模型图解

```
Controller（接收请求）
      │
      ▼
Service（业务逻辑职责）
      │
      ▼
Repository（数据访问职责）
      │
      ▼
Database
```

Java 的世界里，几乎没有"一个类做所有事"这种设计被认为是好设计。
一个系统被拆成一层层"职责明确"的对象：谁负责接收输入、谁负责业务规则、谁负责持久化、
谁负责异常处理——每个对象只对自己的那一份职责负责，通过接口协作完成整体行为。

---

## 核心驱动力详解

- **单一职责**：一个类应该只有一个改变的理由；职责越单一，越容易被复用、测试、替换。
- **接口定义契约，实现承担职责**：调用者只依赖接口（"需要谁做什么"），不关心具体是谁在做。
- **依赖注入（DI）把"找协作者"的职责也转移出去**：类不自己 `new` 依赖，而是由外部"注入"，进一步解耦职责。
- **异常是一种"职责声明"**：受检异常（checked exception）强制调用者显式处理或声明"我把这个职责转交给上层"。

理解了"职责如何拆分、如何协作"，Java 里的接口、抽象类、设计模式、分层架构才会显得"顺理成章"，而不是死记的模板。

---

## 典型代码片段

### 1. 接口与实现分离 —— 依赖契约而非具体实现

```java
public interface NotificationSender {
    void send(String to, String message);
}

public class EmailSender implements NotificationSender {
    @Override
    public void send(String to, String message) {
        System.out.println("Email to " + to + ": " + message);
    }
}

public class OrderService {
    private final NotificationSender sender; // 只依赖接口，不依赖具体的 EmailSender

    public OrderService(NotificationSender sender) {
        this.sender = sender;
    }

    public void notifyCustomer(String email) {
        sender.send(email, "Your order has been placed.");
    }
}
```

**心智模型解读**：`OrderService` 的职责是"下单后要通知客户"，它不关心通知的具体渠道（邮件/短信/推送），
这份职责被拆分给了 `NotificationSender` 的具体实现，`OrderService` 只依赖"能发通知"这个契约。

### 2. 构造函数注入 —— 把"寻找协作者"的职责交给外部

```java
public class OrderController {
    private final OrderService orderService;

    public OrderController(OrderService orderService) { // 依赖由外部传入，而不是自己创建
        this.orderService = orderService;
    }

    public void placeOrder(String email) {
        orderService.notifyCustomer(email);
    }
}

// 组装的职责被集中到一处（比如框架的 IoC 容器，或者手动的组装代码）
NotificationSender sender = new EmailSender();
OrderService service = new OrderService(sender);
OrderController controller = new OrderController(service);
```

**心智模型解读**：`OrderController` 不负责"决定用哪个 `NotificationSender` 实现"，
它只负责"利用注入进来的 `OrderService` 完成自己的职责"。谁组装对象图，是另外一个独立的职责。

### 3. 单一职责的类层次 —— 拆分"下订单"和"计算价格"

```java
public class PriceCalculator {
    public double calculate(Order order) {
        return order.getItems().stream()
                .mapToDouble(i -> i.getPrice() * i.getQuantity())
                .sum();
    }
}

public class OrderValidator {
    public void validate(Order order) {
        if (order.getItems().isEmpty()) {
            throw new IllegalArgumentException("Order must have at least one item");
        }
    }
}

public class OrderProcessor {
    private final OrderValidator validator;
    private final PriceCalculator calculator;

    public OrderProcessor(OrderValidator validator, PriceCalculator calculator) {
        this.validator = validator;
        this.calculator = calculator;
    }

    public double process(Order order) {
        validator.validate(order);       // 校验的职责
        return calculator.calculate(order); // 计算的职责
    }
}
```

**心智模型解读**：如果把"校验规则变化"和"价格计算规则变化"都塞进同一个类，
任何一处修改都要重新审查整个类。拆开后，每个类只对"一个变化的理由"负责。

### 4. 抽象类定义契约，子类承担具体职责

```java
public abstract class ReportGenerator {
    public final String generate(ReportData data) { // 模板方法：流程职责在父类
        String header = buildHeader();
        String body = buildBody(data);
        return header + "\n" + body;
    }

    protected abstract String buildHeader(); // 具体内容的职责交给子类
    protected abstract String buildBody(ReportData data);
}

public class CsvReportGenerator extends ReportGenerator {
    @Override
    protected String buildHeader() { return "id,name,amount"; }

    @Override
    protected String buildBody(ReportData data) { /* 生成 CSV 行 */ return "..."; }
}
```

**心智模型解读**：父类负责"报告生成的固定流程"这一职责，子类负责"具体格式怎么写"这一职责，
两者通过模板方法模式明确分工，而不是每个子类都重复实现一遍完整流程。

### 5. 受检异常 —— 把"处理失败"的职责显式传递给调用者

```java
public class ConfigLoader {
    public Config load(String path) throws IOException { // 明确声明：我可能失败，职责交给你处理
        return parse(Files.readString(Path.of(path)));
    }
}

public class Application {
    public void start() {
        try {
            Config cfg = new ConfigLoader().load("app.yaml");
            // ...
        } catch (IOException e) {
            System.err.println("Failed to load config: " + e.getMessage());
            System.exit(1); // Application 承担了"如何应对失败"的职责
        }
    }
}
```

**心智模型解读**：`throws IOException` 是一份"职责声明"：`ConfigLoader` 说"我不负责决定失败时怎么办，
但我必须告诉你可能会失败"，调用者必须显式接过这份"处理失败"的职责（`catch` 或继续往上抛）。

### 6. Strategy 模式 —— 把"算法选择"的职责从主流程中剥离

```java
public interface DiscountStrategy {
    double apply(double price);
}

public class NoDiscount implements DiscountStrategy {
    public double apply(double price) { return price; }
}

public class PercentageDiscount implements DiscountStrategy {
    private final double percent;
    public PercentageDiscount(double percent) { this.percent = percent; }
    public double apply(double price) { return price * (1 - percent / 100); }
}

public class Checkout {
    private final DiscountStrategy discount;
    public Checkout(DiscountStrategy discount) { this.discount = discount; }
    public double finalPrice(double price) { return discount.apply(price); }
}
```

**心智模型解读**：`Checkout` 的职责是"结算"，不是"决定用什么折扣算法"——
折扣算法本身被抽成独立的职责单元，可以自由替换、组合、测试，不需要改动 `Checkout`。

### 7. Repository 模式 —— 把"数据访问"的职责与业务逻辑分离

```java
public interface UserRepository {
    Optional<User> findById(long id);
    void save(User user);
}

public class JdbcUserRepository implements UserRepository {
    // 具体的 SQL、连接池细节都封装在这里
    public Optional<User> findById(long id) { /* ... */ return Optional.empty(); }
    public void save(User user) { /* ... */ }
}

public class UserService {
    private final UserRepository repository;
    public UserService(UserRepository repository) { this.repository = repository; }

    public void promoteToAdmin(long userId) {
        User user = repository.findById(userId)
                .orElseThrow(() -> new NoSuchElementException("user not found"));
        user.setRole("ADMIN");
        repository.save(user); // UserService 不关心数据到底存在哪、怎么存
    }
}
```

**心智模型解读**：`UserService` 只负责"业务规则"（如何提升用户为管理员），
"数据怎么存取"这个完全不同的职责被交给 `UserRepository`，两者可以独立变化、独立测试。

### 8. Service 层分离 —— 让 Controller 只负责"协调"，不负责"业务规则"

```java
public class OrderApiController {
    private final OrderService orderService;
    public OrderApiController(OrderService orderService) { this.orderService = orderService; }

    public String handlePlaceOrder(OrderRequest request) {
        // Controller 的职责：解析请求、调用 Service、格式化响应——不写业务规则
        Order order = orderService.placeOrder(request.toOrder());
        return "Order #" + order.getId() + " placed";
    }
}
```

**心智模型解读**：如果把折扣计算、库存检查、支付校验都写进 Controller，
一旦要支持另一种入口（比如消息队列触发下单），这些逻辑都要复制一遍。
分层的本质，就是把"业务规则"这份职责集中到一个不依赖具体入口的地方。

### 9. `try-with-resources` / `AutoCloseable` —— 把"资源清理"职责标准化

```java
public class ConnectionPool implements AutoCloseable {
    public void close() {
        System.out.println("releasing all connections");
    }
}

public void run() {
    try (ConnectionPool pool = new ConnectionPool()) {
        // 使用 pool
    } // pool.close() 一定会被调用，即使中间抛出异常
}
```

**心智模型解读**：`AutoCloseable` 是 Java 里对"谁负责清理资源"这个职责的标准化约定，
`try-with-resources` 保证调用者不需要在每条退出路径上手写 `finally { pool.close(); }`。

### 10. Builder 模式 —— 把"复杂对象的构造过程"独立成一份职责

```java
public class HttpRequest {
    private final String url;
    private final String method;
    private final Map<String, String> headers;

    private HttpRequest(Builder b) {
        this.url = b.url;
        this.method = b.method;
        this.headers = b.headers;
    }

    public static class Builder {
        private String url;
        private String method = "GET";
        private Map<String, String> headers = new HashMap<>();

        public Builder url(String url) { this.url = url; return this; }
        public Builder method(String method) { this.method = method; return this; }
        public Builder header(String k, String v) { headers.put(k, v); return this; }
        public HttpRequest build() { return new HttpRequest(this); }
    }
}

HttpRequest req = new HttpRequest.Builder()
        .url("https://api.example.com")
        .method("POST")
        .header("Content-Type", "application/json")
        .build();
```

**心智模型解读**：`HttpRequest` 本身的职责是"表示一个请求"，不是"如何一步步拼装出这个请求"——
后者被拆给专门的 `Builder`，这样 `HttpRequest` 可以保持不可变（immutable），构造逻辑也能独立演化。

### 11. 依赖倒置 —— 高层职责不依赖低层实现细节

```java
public interface PaymentGateway { // 抽象，属于"高层策略"
    void charge(String cardToken, double amount);
}

public class StripeGateway implements PaymentGateway { // 具体实现，属于"低层细节"
    public void charge(String cardToken, double amount) { /* 调用 Stripe SDK */ }
}

public class CheckoutService {
    private final PaymentGateway gateway; // 依赖抽象，不依赖 StripeGateway 这个具体类
    public CheckoutService(PaymentGateway gateway) { this.gateway = gateway; }

    public void checkout(String cardToken, double amount) {
        gateway.charge(cardToken, amount);
    }
}
```

**心智模型解读**：`CheckoutService` 的职责是"结账流程"，它不应该因为"支付渠道从 Stripe 换成 PayPal"
而被迫修改——依赖倒置让"高层业务职责"和"低层技术实现职责"之间只通过接口耦合。

---

## 黄金法则

> **不要想着实现细节，而要想着职责划分。**

设计一个类之前，先问自己："这个类只对一件事负责吗？如果需求变化，
它需要修改的理由是不是只有一个？"而不是先想"这个方法该怎么写"。

---

## 常见误区对比

### 误区一：把所有逻辑塞进一个"万能类"

```java
// 错误心智模型：Controller 同时承担校验、计算、持久化、通知等多重职责
public class OrderController {
    public String placeOrder(OrderRequest request) {
        if (request.getItems().isEmpty()) throw new IllegalArgumentException("empty order");
        double total = request.getItems().stream().mapToDouble(i -> i.getPrice() * i.getQty()).sum();
        // 直接写 JDBC 代码保存订单
        // 直接写发邮件的代码通知用户
        return "OK";
    }
}
```

```java
// Java 习惯写法：把每一份职责拆给专门的协作对象
public class OrderController {
    private final OrderValidator validator;
    private final PriceCalculator calculator;
    private final OrderRepository repository;
    private final NotificationSender sender;
    // 构造函数注入 ...

    public String placeOrder(OrderRequest request) {
        Order order = request.toOrder();
        validator.validate(order);
        double total = calculator.calculate(order);
        repository.save(order);
        sender.send(order.getEmail(), "Order placed, total: " + total);
        return "OK";
    }
}
```

**为什么后者更好**：每个协作对象只对自己那一份职责负责，修改校验规则、价格算法、
存储方式、通知渠道中的任何一个，都不需要触碰其他部分的代码。

### 误区二：吞掉异常，让上层"看起来"没有职责要处理

```java
// 错误心智模型：捕获异常但什么都不做，掩盖了"失败该由谁处理"的问题
public void loadConfig() {
    try {
        Config cfg = configLoader.load("app.yaml");
    } catch (IOException e) {
        // 什么都不做，配置加载失败被悄悄吞掉
    }
}
```

```java
// Java 习惯写法：明确决定这份职责该由谁来处理——记录、转换、或者继续往上抛
public void loadConfig() throws ConfigException {
    try {
        Config cfg = configLoader.load("app.yaml");
    } catch (IOException e) {
        throw new ConfigException("failed to load config", e); // 转成更贴合业务语义的异常，继续传递职责
    }
}
```

**为什么后者更好**：异常处理本身也是一种职责分配——"这个失败该谁负责兜底"必须有明确答案，
悄悄吞掉异常等于让谁都不对这个失败负责，问题会在别的地方以更难排查的形式重新出现。

---

## 快速上手 Checklist

- [ ] 设计一个类之前，能用一句话说清楚它唯一的职责是什么吗？
- [ ] 依赖协作对象时，是通过接口注入，还是在类内部直接 `new` 具体实现？
- [ ] 看到 `throws` 声明，能说出这是把哪一份"处理失败"的职责交给了调用者吗？
- [ ] 遇到"复杂对象构造"或"多种算法可替换"的场景，是否想到 Builder / Strategy 这类职责分离模式？
- [ ] Controller/Service/Repository 分层中，每一层是否只做了该层该做的事，没有越界？

---

上一篇：[C++ —— 生命周期](cpp.md) ・ 下一篇：[JavaScript —— 事件](javascript.md)
