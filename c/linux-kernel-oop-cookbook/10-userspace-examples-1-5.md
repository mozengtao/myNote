# 第五部分（A）：用户空间完整实例 1–5

> 本章完全脱离 Linux Kernel，全部采用普通 C。
> 每个例子都可以 `gcc` 编译，源码在 [`examples/`](examples/) 目录。
> **输出均为本机真实运行结果**（`make run`），不是臆造。

## 本章目标

证明三件事：

1. `struct` 足以表示对象状态与身份
2. 函数指针表（ops）足以表示虚函数与接口
3. `container_of` + 嵌入式 struct 足以表示继承

公用头文件 [`examples/kobj.h`](examples/kobj.h) 提供与内核同构的 `container_of`：

```c
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
```

编译方法：

```bash
cd examples
make
make run
```

---

## 示例1：动物继承：Animal / Dog / Cat

### 为什么这个例子重要

container_of + ops 实现虚函数多态；嵌入 base 即继承

三大支柱在本例中的角色：

| 支柱 | 本例对应 |
|------|----------|
| struct | 基类对象 + 派生对象（嵌入 base） |
| function pointer / ops | 虚函数表 |
| container_of | 从基类指针恢复派生对象 |

### 完整代码（`examples/01_animal.c`）

```c
/*
 * 01_animal.c - Animal inheritance via embedding + ops + container_of
 *
 * Demonstrates:
 *   struct          = object state
 *   animal_ops      = virtual function table
 *   container_of    = recover Dog/Cat from Animal*
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct animal;

struct animal_ops {
	void (*speak)(struct animal *self);
	void (*move)(struct animal *self);
	void (*destroy)(struct animal *self);
};

struct animal {
	const char *name;
	int age;
	const struct animal_ops *ops;
};

static void animal_speak(struct animal *self)
{
	if (self && self->ops && self->ops->speak)
		self->ops->speak(self);
}

static void animal_move(struct animal *self)
{
	if (self && self->ops && self->ops->move)
		self->ops->move(self);
}

static void animal_destroy(struct animal *self)
{
	if (self && self->ops && self->ops->destroy)
		self->ops->destroy(self);
}

/* ---------- Dog (derived) ---------- */

struct dog {
	struct animal base;   /* embedded base = inheritance */
	const char *breed;
	int bone_count;
};

static void dog_speak(struct animal *self)
{
	struct dog *d = container_of(self, struct dog, base);

	printf("[Dog] %s (%s, age %d) says: Woof! Bones=%d\n",
	       self->name, d->breed, self->age, d->bone_count);
}

static void dog_move(struct animal *self)
{
	struct dog *d = container_of(self, struct dog, base);

	printf("[Dog] %s runs on four legs (breed=%s)\n",
	       self->name, d->breed);
}

static void dog_destroy(struct animal *self)
{
	struct dog *d = container_of(self, struct dog, base);

	printf("[Dog] destroying %s\n", self->name);
	free(d);
}

static const struct animal_ops dog_ops = {
	.speak   = dog_speak,
	.move    = dog_move,
	.destroy = dog_destroy,
};

static struct animal *dog_create(const char *name, int age,
				 const char *breed, int bones)
{
	struct dog *d = calloc(1, sizeof(*d));

	if (!d)
		return NULL;
	d->base.name = name;
	d->base.age  = age;
	d->base.ops  = &dog_ops;
	d->breed     = breed;
	d->bone_count = bones;
	return &d->base;
}

/* ---------- Cat (derived) ---------- */

struct cat {
	struct animal base;
	int lives;
	int climb_height;
};

static void cat_speak(struct animal *self)
{
	struct cat *c = container_of(self, struct cat, base);

	printf("[Cat] %s (age %d, lives=%d) says: Meow!\n",
	       self->name, self->age, c->lives);
}

static void cat_move(struct animal *self)
{
	struct cat *c = container_of(self, struct cat, base);

	printf("[Cat] %s climbs to %d meters\n",
	       self->name, c->climb_height);
}

static void cat_destroy(struct animal *self)
{
	struct cat *c = container_of(self, struct cat, base);

	printf("[Cat] destroying %s\n", self->name);
	free(c);
}

static const struct animal_ops cat_ops = {
	.speak   = cat_speak,
	.move    = cat_move,
	.destroy = cat_destroy,
};

static struct animal *cat_create(const char *name, int age,
				 int lives, int climb)
{
	struct cat *c = calloc(1, sizeof(*c));

	if (!c)
		return NULL;
	c->base.name = name;
	c->base.age  = age;
	c->base.ops  = &cat_ops;
	c->lives     = lives;
	c->climb_height = climb;
	return &c->base;
}

/* ---------- Framework: polymorphic dispatch ---------- */

static void zoo_show(struct animal **animals, int n)
{
	int i;

	printf("--- Zoo polymorphic show ---\n");
	for (i = 0; i < n; i++) {
		animal_speak(animals[i]);
		animal_move(animals[i]);
	}
}

int main(void)
{
	struct animal *zoo[3];
	int i;

	printf("=== 01 Animal Inheritance ===\n");
	printf("offsetof(struct dog, base) = %zu\n",
	       offsetof(struct dog, base));
	printf("offsetof(struct cat, base) = %zu\n",
	       offsetof(struct cat, base));

	zoo[0] = dog_create("Rex", 5, "Labrador", 3);
	zoo[1] = cat_create("Mimi", 3, 9, 4);
	zoo[2] = dog_create("Buddy", 2, "Beagle", 1);

	zoo_show(zoo, 3);

	printf("--- Cleanup via virtual destroy ---\n");
	for (i = 0; i < 3; i++)
		animal_destroy(zoo[i]);

	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 01 Animal Inheritance ===
offsetof(struct dog, base) = 0
offsetof(struct cat, base) = 0
--- Zoo polymorphic show ---
[Dog] Rex (Labrador, age 5) says: Woof! Bones=3
[Dog] Rex runs on four legs (breed=Labrador)
[Cat] Mimi (age 3, lives=9) says: Meow!
[Cat] Mimi climbs to 4 meters
[Dog] Buddy (Beagle, age 2) says: Woof! Bones=1
[Dog] Buddy runs on four legs (breed=Beagle)
--- Cleanup via virtual destroy ---
[Dog] destroying Rex
[Cat] destroying Mimi
[Dog] destroying Buddy
Done.
```

### 若写成 C++ 会是什么样

```cpp
class Animal {
public:
  virtual void speak() = 0;
  virtual void move() = 0;
  virtual ~Animal() {}
};
class Dog : public Animal {
  string breed; int bones;
  void speak() override;
  void move() override;
};
```

C 版本的关键差别：

- `this` 变成显式的 `self` 参数
- `virtual` 变成 ops 表里的函数指针
- `static_cast<Derived*>(base)` 变成 `container_of(base, struct derived, member)`
- 析构函数变成 ops 里的 `destroy` / `release`

### 心智模型

```
+------------------+
| derived object   |
| +--------------+ |
| | base (embed) | |  <--- 对外以 base* 传递
| +--------------+ |
| | extra fields | |
+------------------+
         ^
         | container_of
         |
      base* from framework callback
```

### 本例总结

> 这里体现了 OO Design 的：**继承（Embedding）+ 多态（ops 动态分派）+ 封装（对外只暴露 base 接口）**。

---

## 示例2：GUI Widget：Button / Label / Window

### 为什么这个例子重要

组合对象树 + 多态 draw/on_click；Window 作为 Composite

三大支柱在本例中的角色：

| 支柱 | 本例对应 |
|------|----------|
| struct | 基类对象 + 派生对象（嵌入 base） |
| function pointer / ops | 虚函数表 |
| container_of | 从基类指针恢复派生对象 |

### 完整代码（`examples/02_widget.c`）

```c
/*
 * 02_widget.c - GUI Widget hierarchy (Button / Label / Window)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct widget;

struct widget_ops {
	void (*draw)(struct widget *self);
	void (*on_click)(struct widget *self);
	void (*destroy)(struct widget *self);
};

struct widget {
	const char *id;
	int x, y, w, h;
	const struct widget_ops *ops;
};

static void widget_draw(struct widget *self)
{
	if (self && self->ops && self->ops->draw)
		self->ops->draw(self);
}

static void widget_click(struct widget *self)
{
	if (self && self->ops && self->ops->on_click)
		self->ops->on_click(self);
}

static void widget_destroy(struct widget *self)
{
	if (self && self->ops && self->ops->destroy)
		self->ops->destroy(self);
}

/* ---------- Button ---------- */

struct button {
	struct widget base;
	const char *label;
	int pressed;
};

static void button_draw(struct widget *self)
{
	struct button *b = container_of(self, struct button, base);

	printf("[Button] id=%s pos=(%d,%d) size=%dx%d label=\"%s\" pressed=%d\n",
	       self->id, self->x, self->y, self->w, self->h,
	       b->label, b->pressed);
}

static void button_click(struct widget *self)
{
	struct button *b = container_of(self, struct button, base);

	b->pressed++;
	printf("[Button] %s clicked -> pressed=%d\n", b->label, b->pressed);
}

static void button_destroy(struct widget *self)
{
	free(container_of(self, struct button, base));
}

static const struct widget_ops button_ops = {
	.draw     = button_draw,
	.on_click = button_click,
	.destroy  = button_destroy,
};

static struct widget *button_create(const char *id, int x, int y,
				    int w, int h, const char *label)
{
	struct button *b = calloc(1, sizeof(*b));

	if (!b)
		return NULL;
	b->base.id  = id;
	b->base.x   = x;
	b->base.y   = y;
	b->base.w   = w;
	b->base.h   = h;
	b->base.ops = &button_ops;
	b->label    = label;
	return &b->base;
}

/* ---------- Label ---------- */

struct label {
	struct widget base;
	const char *text;
};

static void label_draw(struct widget *self)
{
	struct label *l = container_of(self, struct label, base);

	printf("[Label] id=%s pos=(%d,%d) text=\"%s\"\n",
	       self->id, self->x, self->y, l->text);
}

static void label_click(struct widget *self)
{
	(void)self;
	printf("[Label] click ignored (not interactive)\n");
}

static void label_destroy(struct widget *self)
{
	free(container_of(self, struct label, base));
}

static const struct widget_ops label_ops = {
	.draw     = label_draw,
	.on_click = label_click,
	.destroy  = label_destroy,
};

static struct widget *label_create(const char *id, int x, int y,
				   const char *text)
{
	struct label *l = calloc(1, sizeof(*l));

	if (!l)
		return NULL;
	l->base.id  = id;
	l->base.x   = x;
	l->base.y   = y;
	l->base.w   = 100;
	l->base.h   = 20;
	l->base.ops = &label_ops;
	l->text     = text;
	return &l->base;
}

/* ---------- Window (composite) ---------- */

struct window {
	struct widget base;
	const char *title;
	struct widget *children[8];
	int n_children;
};

static void window_draw(struct widget *self)
{
	struct window *win = container_of(self, struct window, base);
	int i;

	printf("[Window] id=%s title=\"%s\" children=%d\n",
	       self->id, win->title, win->n_children);
	for (i = 0; i < win->n_children; i++)
		widget_draw(win->children[i]);
}

static void window_click(struct widget *self)
{
	struct window *win = container_of(self, struct window, base);
	int i;

	printf("[Window] forwarding click to children\n");
	for (i = 0; i < win->n_children; i++)
		widget_click(win->children[i]);
}

static void window_destroy(struct widget *self)
{
	struct window *win = container_of(self, struct window, base);
	int i;

	for (i = 0; i < win->n_children; i++)
		widget_destroy(win->children[i]);
	free(win);
}

static const struct widget_ops window_ops = {
	.draw     = window_draw,
	.on_click = window_click,
	.destroy  = window_destroy,
};

static struct widget *window_create(const char *id, const char *title)
{
	struct window *win = calloc(1, sizeof(*win));

	if (!win)
		return NULL;
	win->base.id  = id;
	win->base.ops = &window_ops;
	win->title    = title;
	return &win->base;
}

static void window_add(struct widget *self, struct widget *child)
{
	struct window *win = container_of(self, struct window, base);

	if (win->n_children < 8)
		win->children[win->n_children++] = child;
}

int main(void)
{
	struct widget *root, *btn, *lbl;

	printf("=== 02 GUI Widget ===\n");
	root = window_create("main", "Demo App");
	btn  = button_create("ok", 10, 40, 80, 30, "OK");
	lbl  = label_create("hint", 10, 10, "Click the button");

	window_add(root, lbl);
	window_add(root, btn);

	printf("--- Draw tree ---\n");
	widget_draw(root);

	printf("--- Click tree ---\n");
	widget_click(root);

	printf("--- Destroy tree ---\n");
	widget_destroy(root);
	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 02 GUI Widget ===
--- Draw tree ---
[Window] id=main title="Demo App" children=2
[Label] id=hint pos=(10,10) text="Click the button"
[Button] id=ok pos=(10,40) size=80x30 label="OK" pressed=0
--- Click tree ---
[Window] forwarding click to children
[Label] click ignored (not interactive)
[Button] OK clicked -> pressed=1
--- Destroy tree ---
Done.
```

### 若写成 C++ 会是什么样

```cpp
class Widget {
public:
  virtual void draw() = 0;
  virtual void on_click() = 0;
  virtual ~Widget() {}
};
class Window : public Widget {
  vector<Widget*> children;
  void draw() override; // draw self then children
};
```

C 版本的关键差别：

- `this` 变成显式的 `self` 参数
- `virtual` 变成 ops 表里的函数指针
- `static_cast<Derived*>(base)` 变成 `container_of(base, struct derived, member)`
- 析构函数变成 ops 里的 `destroy` / `release`

### 心智模型

```
+------------------+
| derived object   |
| +--------------+ |
| | base (embed) | |  <--- 对外以 base* 传递
| +--------------+ |
| | extra fields | |
+------------------+
         ^
         | container_of
         |
      base* from framework callback
```

### 本例总结

> 这里体现了 OO Design 的：**继承（Embedding）+ 多态（ops 动态分派）+ 封装（对外只暴露 base 接口）**。

---

## 示例3：Shape：Circle / Rectangle / Triangle

### 为什么这个例子重要

经典多态：同一 shape* 数组，运行时分派 area/draw

三大支柱在本例中的角色：

| 支柱 | 本例对应 |
|------|----------|
| struct | 基类对象 + 派生对象（嵌入 base） |
| function pointer / ops | 虚函数表 |
| container_of | 从基类指针恢复派生对象 |

### 完整代码（`examples/03_shape.c`）

```c
/*
 * 03_shape.c - Shape hierarchy with area/draw virtual methods
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "kobj.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct shape;

struct shape_ops {
	double (*area)(struct shape *self);
	void (*draw)(struct shape *self);
	void (*destroy)(struct shape *self);
};

struct shape {
	const char *name;
	const struct shape_ops *ops;
};

static double shape_area(struct shape *self)
{
	return (self && self->ops && self->ops->area)
		? self->ops->area(self) : 0.0;
}

static void shape_draw(struct shape *self)
{
	if (self && self->ops && self->ops->draw)
		self->ops->draw(self);
}

static void shape_destroy(struct shape *self)
{
	if (self && self->ops && self->ops->destroy)
		self->ops->destroy(self);
}

/* Circle */
struct circle {
	struct shape base;
	double radius;
};

static double circle_area(struct shape *self)
{
	struct circle *c = container_of(self, struct circle, base);

	return M_PI * c->radius * c->radius;
}

static void circle_draw(struct shape *self)
{
	struct circle *c = container_of(self, struct circle, base);

	printf("[Circle] %s r=%.2f area=%.2f\n",
	       self->name, c->radius, shape_area(self));
}

static void circle_destroy(struct shape *self)
{
	free(container_of(self, struct circle, base));
}

static const struct shape_ops circle_ops = {
	.area    = circle_area,
	.draw    = circle_draw,
	.destroy = circle_destroy,
};

static struct shape *circle_create(const char *name, double r)
{
	struct circle *c = calloc(1, sizeof(*c));

	if (!c)
		return NULL;
	c->base.name = name;
	c->base.ops  = &circle_ops;
	c->radius    = r;
	return &c->base;
}

/* Rectangle */
struct rectangle {
	struct shape base;
	double width, height;
};

static double rect_area(struct shape *self)
{
	struct rectangle *r = container_of(self, struct rectangle, base);

	return r->width * r->height;
}

static void rect_draw(struct shape *self)
{
	struct rectangle *r = container_of(self, struct rectangle, base);

	printf("[Rectangle] %s w=%.2f h=%.2f area=%.2f\n",
	       self->name, r->width, r->height, shape_area(self));
}

static void rect_destroy(struct shape *self)
{
	free(container_of(self, struct rectangle, base));
}

static const struct shape_ops rect_ops = {
	.area    = rect_area,
	.draw    = rect_draw,
	.destroy = rect_destroy,
};

static struct shape *rect_create(const char *name, double w, double h)
{
	struct rectangle *r = calloc(1, sizeof(*r));

	if (!r)
		return NULL;
	r->base.name = name;
	r->base.ops  = &rect_ops;
	r->width     = w;
	r->height    = h;
	return &r->base;
}

/* Triangle */
struct triangle {
	struct shape base;
	double a, b, c;
};

static double tri_area(struct shape *self)
{
	struct triangle *t = container_of(self, struct triangle, base);
	double s = (t->a + t->b + t->c) / 2.0;

	return sqrt(s * (s - t->a) * (s - t->b) * (s - t->c));
}

static void tri_draw(struct shape *self)
{
	struct triangle *t = container_of(self, struct triangle, base);

	printf("[Triangle] %s sides=%.1f,%.1f,%.1f area=%.2f\n",
	       self->name, t->a, t->b, t->c, shape_area(self));
}

static void tri_destroy(struct shape *self)
{
	free(container_of(self, struct triangle, base));
}

static const struct shape_ops tri_ops = {
	.area    = tri_area,
	.draw    = tri_draw,
	.destroy = tri_destroy,
};

static struct shape *tri_create(const char *name, double a, double b, double c)
{
	struct triangle *t = calloc(1, sizeof(*t));

	if (!t)
		return NULL;
	t->base.name = name;
	t->base.ops  = &tri_ops;
	t->a = a;
	t->b = b;
	t->c = c;
	return &t->base;
}

int main(void)
{
	struct shape *shapes[3];
	double total = 0.0;
	int i;

	printf("=== 03 Shape Polymorphism ===\n");
	shapes[0] = circle_create("C1", 2.0);
	shapes[1] = rect_create("R1", 3.0, 4.0);
	shapes[2] = tri_create("T1", 3.0, 4.0, 5.0);

	for (i = 0; i < 3; i++) {
		shape_draw(shapes[i]);
		total += shape_area(shapes[i]);
	}
	printf("Total area = %.2f\n", total);

	for (i = 0; i < 3; i++)
		shape_destroy(shapes[i]);
	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 03 Shape Polymorphism ===
[Circle] C1 r=2.00 area=12.57
[Rectangle] R1 w=3.00 h=4.00 area=12.00
[Triangle] T1 sides=3.0,4.0,5.0 area=6.00
Total area = 30.57
Done.
```

### 若写成 C++ 会是什么样

```cpp
class Shape {
public:
  virtual double area() const = 0;
  virtual void draw() const = 0;
  virtual ~Shape() {}
};
class Circle : public Shape { double r; double area() const override; };
```

C 版本的关键差别：

- `this` 变成显式的 `self` 参数
- `virtual` 变成 ops 表里的函数指针
- `static_cast<Derived*>(base)` 变成 `container_of(base, struct derived, member)`
- 析构函数变成 ops 里的 `destroy` / `release`

### 心智模型

```
+------------------+
| derived object   |
| +--------------+ |
| | base (embed) | |  <--- 对外以 base* 传递
| +--------------+ |
| | extra fields | |
+------------------+
         ^
         | container_of
         |
      base* from framework callback
```

### 本例总结

> 这里体现了 OO Design 的：**继承（Embedding）+ 多态（ops 动态分派）+ 封装（对外只暴露 base 接口）**。

---

## 示例4：Plugin Framework

### 为什么这个例子重要

插件接口 + Loader 注册表；框架不知具体插件类型

三大支柱在本例中的角色：

| 支柱 | 本例对应 |
|------|----------|
| struct | 基类对象 + 派生对象（嵌入 base） |
| function pointer / ops | 虚函数表 |
| container_of | 从基类指针恢复派生对象 |

### 完整代码（`examples/04_plugin.c`）

```c
/*
 * 04_plugin.c - Plugin framework: interface + loader + dynamic dispatch
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct plugin;

struct plugin_ops {
	int  (*init)(struct plugin *self);
	void (*execute)(struct plugin *self, const char *arg);
	void (*shutdown)(struct plugin *self);
};

struct plugin {
	const char *name;
	const char *version;
	const struct plugin_ops *ops;
};

/* Registry */
#define MAX_PLUGINS 8

struct plugin_registry {
	struct plugin *slots[MAX_PLUGINS];
	int count;
};

static void registry_register(struct plugin_registry *reg, struct plugin *p)
{
	if (reg->count >= MAX_PLUGINS)
		return;
	printf("[Loader] register plugin \"%s\" v%s\n", p->name, p->version);
	reg->slots[reg->count++] = p;
	if (p->ops && p->ops->init)
		p->ops->init(p);
}

static void registry_run_all(struct plugin_registry *reg, const char *arg)
{
	int i;

	for (i = 0; i < reg->count; i++) {
		struct plugin *p = reg->slots[i];

		printf("[Loader] execute %s(%s)\n", p->name, arg);
		if (p->ops && p->ops->execute)
			p->ops->execute(p, arg);
	}
}

static void registry_shutdown(struct plugin_registry *reg)
{
	int i;

	for (i = 0; i < reg->count; i++) {
		struct plugin *p = reg->slots[i];

		if (p->ops && p->ops->shutdown)
			p->ops->shutdown(p);
	}
	reg->count = 0;
}

/* ---- Logger plugin ---- */

struct logger_plugin {
	struct plugin base;
	int log_count;
};

static int logger_init(struct plugin *self)
{
	struct logger_plugin *lp = container_of(self, struct logger_plugin, base);

	lp->log_count = 0;
	printf("  [Logger] init ok\n");
	return 0;
}

static void logger_execute(struct plugin *self, const char *arg)
{
	struct logger_plugin *lp = container_of(self, struct logger_plugin, base);

	lp->log_count++;
	printf("  [Logger] LOG#%d: %s\n", lp->log_count, arg);
}

static void logger_shutdown(struct plugin *self)
{
	struct logger_plugin *lp = container_of(self, struct logger_plugin, base);

	printf("  [Logger] shutdown (logged %d msgs)\n", lp->log_count);
}

static const struct plugin_ops logger_ops = {
	.init     = logger_init,
	.execute  = logger_execute,
	.shutdown = logger_shutdown,
};

/* ---- Crypto plugin ---- */

struct crypto_plugin {
	struct plugin base;
	const char *algo;
};

static int crypto_init(struct plugin *self)
{
	struct crypto_plugin *cp = container_of(self, struct crypto_plugin, base);

	printf("  [Crypto] init algo=%s\n", cp->algo);
	return 0;
}

static void crypto_execute(struct plugin *self, const char *arg)
{
	struct crypto_plugin *cp = container_of(self, struct crypto_plugin, base);

	printf("  [Crypto] encrypt(\"%s\") with %s -> \"%s#hash\"\n",
	       arg, cp->algo, arg);
}

static void crypto_shutdown(struct plugin *self)
{
	(void)self;
	printf("  [Crypto] shutdown\n");
}

static const struct plugin_ops crypto_ops = {
	.init     = crypto_init,
	.execute  = crypto_execute,
	.shutdown = crypto_shutdown,
};

/* ---- Metrics plugin ---- */

struct metrics_plugin {
	struct plugin base;
	int samples;
};

static int metrics_init(struct plugin *self)
{
	struct metrics_plugin *mp =
		container_of(self, struct metrics_plugin, base);

	mp->samples = 0;
	printf("  [Metrics] init ok\n");
	return 0;
}

static void metrics_execute(struct plugin *self, const char *arg)
{
	struct metrics_plugin *mp =
		container_of(self, struct metrics_plugin, base);

	mp->samples++;
	printf("  [Metrics] sample#%d event=\"%s\"\n", mp->samples, arg);
}

static void metrics_shutdown(struct plugin *self)
{
	struct metrics_plugin *mp =
		container_of(self, struct metrics_plugin, base);

	printf("  [Metrics] shutdown (samples=%d)\n", mp->samples);
}

static const struct plugin_ops metrics_ops = {
	.init     = metrics_init,
	.execute  = metrics_execute,
	.shutdown = metrics_shutdown,
};

int main(void)
{
	struct plugin_registry reg = {0};
	struct logger_plugin  logger = {
		.base = { .name = "logger", .version = "1.0", .ops = &logger_ops },
	};
	struct crypto_plugin  crypto = {
		.base = { .name = "crypto", .version = "2.1", .ops = &crypto_ops },
		.algo = "AES-256",
	};
	struct metrics_plugin metrics = {
		.base = { .name = "metrics", .version = "0.9", .ops = &metrics_ops },
	};

	printf("=== 04 Plugin Framework ===\n");
	registry_register(&reg, &logger.base);
	registry_register(&reg, &crypto.base);
	registry_register(&reg, &metrics.base);

	registry_run_all(&reg, "hello-world");
	registry_run_all(&reg, "second-event");

	registry_shutdown(&reg);
	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 04 Plugin Framework ===
[Loader] register plugin "logger" v1.0
  [Logger] init ok
[Loader] register plugin "crypto" v2.1
  [Crypto] init algo=AES-256
[Loader] register plugin "metrics" v0.9
  [Metrics] init ok
[Loader] execute logger(hello-world)
  [Logger] LOG#1: hello-world
[Loader] execute crypto(hello-world)
  [Crypto] encrypt("hello-world") with AES-256 -> "hello-world#hash"
[Loader] execute metrics(hello-world)
  [Metrics] sample#1 event="hello-world"
[Loader] execute logger(second-event)
  [Logger] LOG#2: second-event
[Loader] execute crypto(second-event)
  [Crypto] encrypt("second-event") with AES-256 -> "second-event#hash"
[Loader] execute metrics(second-event)
  [Metrics] sample#2 event="second-event"
  [Logger] shutdown (logged 2 msgs)
  [Crypto] shutdown
  [Metrics] shutdown (samples=2)
Done.
```

### 若写成 C++ 会是什么样

```cpp
struct Plugin {
  virtual int init() = 0;
  virtual void execute(const string& arg) = 0;
  virtual void shutdown() = 0;
};
class Registry { void registerPlugin(Plugin*); void runAll(string); };
```

C 版本的关键差别：

- `this` 变成显式的 `self` 参数
- `virtual` 变成 ops 表里的函数指针
- `static_cast<Derived*>(base)` 变成 `container_of(base, struct derived, member)`
- 析构函数变成 ops 里的 `destroy` / `release`

### 心智模型

```
+------------------+
| derived object   |
| +--------------+ |
| | base (embed) | |  <--- 对外以 base* 传递
| +--------------+ |
| | extra fields | |
+------------------+
         ^
         | container_of
         |
      base* from framework callback
```

### 本例总结

> 这里体现了 OO Design 的：**继承（Embedding）+ 多态（ops 动态分派）+ 封装（对外只暴露 base 接口）**。

---

## 示例5：网络协议：TCP / UDP / QUIC

### 为什么这个例子重要

协议族 ops：connect/send/close 动态绑定

三大支柱在本例中的角色：

| 支柱 | 本例对应 |
|------|----------|
| struct | 基类对象 + 派生对象（嵌入 base） |
| function pointer / ops | 虚函数表 |
| container_of | 从基类指针恢复派生对象 |

### 完整代码（`examples/05_protocol.c`）

```c
/*
 * 05_protocol.c - Network protocol family: TCP / UDP / QUIC
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct proto;

struct proto_ops {
	int  (*connect)(struct proto *self, const char *peer);
	int  (*send)(struct proto *self, const char *data, int len);
	int  (*recv)(struct proto *self, char *buf, int len);
	void (*close)(struct proto *self);
};

struct proto {
	const char *name;
	int fd;
	const struct proto_ops *ops;
};

static int proto_connect(struct proto *self, const char *peer)
{
	return (self && self->ops && self->ops->connect)
		? self->ops->connect(self, peer) : -1;
}

static int proto_send(struct proto *self, const char *data, int len)
{
	return (self && self->ops && self->ops->send)
		? self->ops->send(self, data, len) : -1;
}

static void proto_close(struct proto *self)
{
	if (self && self->ops && self->ops->close)
		self->ops->close(self);
}

/* TCP */
struct tcp_proto {
	struct proto base;
	int seq;
	int ack;
};

static int tcp_connect(struct proto *self, const char *peer)
{
	struct tcp_proto *t = container_of(self, struct tcp_proto, base);

	self->fd = 100;
	t->seq = 1;
	t->ack = 0;
	printf("[TCP] 3-way handshake to %s fd=%d seq=%d\n",
	       peer, self->fd, t->seq);
	return 0;
}

static int tcp_send(struct proto *self, const char *data, int len)
{
	struct tcp_proto *t = container_of(self, struct tcp_proto, base);

	t->seq += len;
	printf("[TCP] send %d bytes seq=%d data=\"%.*s\"\n",
	       len, t->seq, len, data);
	return len;
}

static void tcp_close(struct proto *self)
{
	printf("[TCP] FIN/ACK close fd=%d\n", self->fd);
	self->fd = -1;
}

static const struct proto_ops tcp_ops = {
	.connect = tcp_connect,
	.send    = tcp_send,
	.close   = tcp_close,
};

/* UDP */
struct udp_proto {
	struct proto base;
	int datagrams;
};

static int udp_connect(struct proto *self, const char *peer)
{
	struct udp_proto *u = container_of(self, struct udp_proto, base);

	self->fd = 200;
	u->datagrams = 0;
	printf("[UDP] bind/connectless peer=%s fd=%d\n", peer, self->fd);
	return 0;
}

static int udp_send(struct proto *self, const char *data, int len)
{
	struct udp_proto *u = container_of(self, struct udp_proto, base);

	u->datagrams++;
	printf("[UDP] datagram#%d len=%d data=\"%.*s\"\n",
	       u->datagrams, len, len, data);
	return len;
}

static void udp_close(struct proto *self)
{
	struct udp_proto *u = container_of(self, struct udp_proto, base);

	printf("[UDP] close fd=%d sent=%d datagrams\n",
	       self->fd, u->datagrams);
	self->fd = -1;
}

static const struct proto_ops udp_ops = {
	.connect = udp_connect,
	.send    = udp_send,
	.close   = udp_close,
};

/* QUIC */
struct quic_proto {
	struct proto base;
	int stream_id;
	int encrypted;
};

static int quic_connect(struct proto *self, const char *peer)
{
	struct quic_proto *q = container_of(self, struct quic_proto, base);

	self->fd = 300;
	q->stream_id = 0;
	q->encrypted = 1;
	printf("[QUIC] TLS1.3+QUIC handshake to %s fd=%d encrypted=%d\n",
	       peer, self->fd, q->encrypted);
	return 0;
}

static int quic_send(struct proto *self, const char *data, int len)
{
	struct quic_proto *q = container_of(self, struct quic_proto, base);

	q->stream_id++;
	printf("[QUIC] stream=%d len=%d data=\"%.*s\"\n",
	       q->stream_id, len, len, data);
	return len;
}

static void quic_close(struct proto *self)
{
	printf("[QUIC] CONNECTION_CLOSE fd=%d\n", self->fd);
	self->fd = -1;
}

static const struct proto_ops quic_ops = {
	.connect = quic_connect,
	.send    = quic_send,
	.close   = quic_close,
};

static void exercise(struct proto *p, const char *peer, const char *msg)
{
	proto_connect(p, peer);
	proto_send(p, msg, (int)strlen(msg));
	proto_close(p);
	printf("\n");
}

int main(void)
{
	struct tcp_proto  tcp  = { .base = { .name = "tcp",  .ops = &tcp_ops  } };
	struct udp_proto  udp  = { .base = { .name = "udp",  .ops = &udp_ops  } };
	struct quic_proto quic = { .base = { .name = "quic", .ops = &quic_ops } };

	printf("=== 05 Network Protocol ===\n");
	exercise(&tcp.base,  "10.0.0.1:443", "GET /");
	exercise(&udp.base,  "8.8.8.8:53",   "DNS-query");
	exercise(&quic.base, "1.1.1.1:443",  "HTTP/3");
	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 05 Network Protocol ===
[TCP] 3-way handshake to 10.0.0.1:443 fd=100 seq=1
[TCP] send 5 bytes seq=6 data="GET /"
[TCP] FIN/ACK close fd=100

[UDP] bind/connectless peer=8.8.8.8:53 fd=200
[UDP] datagram#1 len=9 data="DNS-query"
[UDP] close fd=200 sent=1 datagrams

[QUIC] TLS1.3+QUIC handshake to 1.1.1.1:443 fd=300 encrypted=1
[QUIC] stream=1 len=6 data="HTTP/3"
[QUIC] CONNECTION_CLOSE fd=300

Done.
```

### 若写成 C++ 会是什么样

```cpp
struct Proto {
  virtual int connect(string peer) = 0;
  virtual int send(const char*, int) = 0;
  virtual void close() = 0;
};
class TcpProto : public Proto { int seq, ack; };
```

C 版本的关键差别：

- `this` 变成显式的 `self` 参数
- `virtual` 变成 ops 表里的函数指针
- `static_cast<Derived*>(base)` 变成 `container_of(base, struct derived, member)`
- 析构函数变成 ops 里的 `destroy` / `release`

### 心智模型

```
+------------------+
| derived object   |
| +--------------+ |
| | base (embed) | |  <--- 对外以 base* 传递
| +--------------+ |
| | extra fields | |
+------------------+
         ^
         | container_of
         |
      base* from framework callback
```

### 本例总结

> 这里体现了 OO Design 的：**继承（Embedding）+ 多态（ops 动态分派）+ 封装（对外只暴露 base 接口）**。

---

