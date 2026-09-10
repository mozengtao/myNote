/*
 * vtable_shape_polymorphism.c
 *
 * 场景：用手写"ops table"实现 C 语言里的多态对象系统 —— 这正是 Kernel
 * struct file_operations / struct inode_operations 的通用范式：
 *
 *   struct shape {
 *       const struct shape_ops *ops;   // <-- 手写 vtable 指针
 *       ...
 *   };
 *
 * 一个 "对象" = 数据 + 一个指向"只读、共享、常量" ops 表的指针。
 * 不同类型的对象共享同一份 ops 表实例（static const），天然 cache 友好，
 * 且完全没有 C++ 虚函数表那样的隐藏开销——一切都是显式代码。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o vtable_shape_polymorphism vtable_shape_polymorphism.c
 * 运行：
 *   ./vtable_shape_polymorphism
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* 1. 契约：ops table 的形状（Shape）—— 类似 struct file_operations   */
/* ------------------------------------------------------------------ */

struct shape;

struct shape_ops {
	double (*area)(const struct shape *self);
	double (*perimeter)(const struct shape *self);
	void   (*describe)(const struct shape *self, char *buf, size_t bufsz);
	void   (*destroy)(struct shape *self);   /* 类似 f_op->release() */
};

struct shape {
	const struct shape_ops *ops;   /* <-- 手写 vtable 指针，注入点 */
	const char *type_name;
};

/* Core 侧的"通用操作" —— 完全不知道具体是圆形还是矩形 */
static double shape_area(const struct shape *s)      { return s->ops->area(s); }
static double shape_perimeter(const struct shape *s)  { return s->ops->perimeter(s); }
static void   shape_describe(const struct shape *s, char *buf, size_t n) { s->ops->describe(s, buf, n); }
static void   shape_destroy(struct shape *s)          { s->ops->destroy(s); }

/* ------------------------------------------------------------------ */
/* 2. 具体实现 A：圆形                                                */
/* ------------------------------------------------------------------ */

struct circle {
	struct shape base;   /* 必须放在第一个字段，或使用 container_of（见 03 篇） */
	double radius;
};

static double circle_area(const struct shape *self)
{
	const struct circle *c = (const struct circle *)self;
	return M_PI * c->radius * c->radius;
}

static double circle_perimeter(const struct shape *self)
{
	const struct circle *c = (const struct circle *)self;
	return 2 * M_PI * c->radius;
}

static void circle_describe(const struct shape *self, char *buf, size_t n)
{
	const struct circle *c = (const struct circle *)self;
	snprintf(buf, n, "circle(r=%.2f)", c->radius);
}

static void circle_destroy(struct shape *self)
{
	printf("  [circle_ops] destroying circle at %p\n", (void *)self);
	free(self);
}

static const struct shape_ops circle_ops = {
	.area       = circle_area,
	.perimeter  = circle_perimeter,
	.describe   = circle_describe,
	.destroy    = circle_destroy,
};

static struct shape *circle_create(double radius)
{
	struct circle *c = malloc(sizeof(*c));
	c->base.ops = &circle_ops;      /* <-- 注入：把 vtable 装进对象 */
	c->base.type_name = "circle";
	c->radius = radius;
	return &c->base;
}

/* ------------------------------------------------------------------ */
/* 3. 具体实现 B：矩形                                                */
/* ------------------------------------------------------------------ */

struct rectangle {
	struct shape base;
	double w, h;
};

static double rect_area(const struct shape *self)
{
	const struct rectangle *r = (const struct rectangle *)self;
	return r->w * r->h;
}

static double rect_perimeter(const struct shape *self)
{
	const struct rectangle *r = (const struct rectangle *)self;
	return 2 * (r->w + r->h);
}

static void rect_describe(const struct shape *self, char *buf, size_t n)
{
	const struct rectangle *r = (const struct rectangle *)self;
	snprintf(buf, n, "rectangle(%.2fx%.2f)", r->w, r->h);
}

static void rect_destroy(struct shape *self)
{
	printf("  [rect_ops] destroying rectangle at %p\n", (void *)self);
	free(self);
}

static const struct shape_ops rect_ops = {
	.area       = rect_area,
	.perimeter  = rect_perimeter,
	.describe   = rect_describe,
	.destroy    = rect_destroy,
};

static struct shape *rectangle_create(double w, double h)
{
	struct rectangle *r = malloc(sizeof(*r));
	r->base.ops = &rect_ops;
	r->base.type_name = "rectangle";
	r->w = w; r->h = h;
	return &r->base;
}

/* ------------------------------------------------------------------ */
/* 4. 具体实现 C：三角形（等边），演示"新增类型不改核心代码"           */
/* ------------------------------------------------------------------ */

struct triangle {
	struct shape base;
	double side;
};

static double triangle_area(const struct shape *self)
{
	const struct triangle *t = (const struct triangle *)self;
	return (sqrt(3.0) / 4.0) * t->side * t->side;
}

static double triangle_perimeter(const struct shape *self)
{
	const struct triangle *t = (const struct triangle *)self;
	return 3 * t->side;
}

static void triangle_describe(const struct shape *self, char *buf, size_t n)
{
	const struct triangle *t = (const struct triangle *)self;
	snprintf(buf, n, "equilateral-triangle(side=%.2f)", t->side);
}

static void triangle_destroy(struct shape *self)
{
	printf("  [triangle_ops] destroying triangle at %p\n", (void *)self);
	free(self);
}

/* 注意：这里没有实现 perimeter 的独立测试路径，但字段仍必须填，
 * 缺字段会在 Failure 小节讨论（NULL 字段导致 crash）*/
static const struct shape_ops triangle_ops = {
	.area       = triangle_area,
	.perimeter  = triangle_perimeter,
	.describe   = triangle_describe,
	.destroy    = triangle_destroy,
};

static struct shape *triangle_create(double side)
{
	struct triangle *t = malloc(sizeof(*t));
	t->base.ops = &triangle_ops;
	t->base.type_name = "triangle";
	t->side = side;
	return &t->base;
}

/* ------------------------------------------------------------------ */
/* 5. Core 逻辑：完全不知道任何具体类型，只通过 ops table 操作          */
/* ------------------------------------------------------------------ */

static void print_shape_report(struct shape *s)
{
	char buf[128];
	shape_describe(s, buf, sizeof(buf));
	printf("[core] %-32s area=%.3f perimeter=%.3f (type=%s)\n",
	       buf, shape_area(s), shape_perimeter(s), s->type_name);
}

int main(void)
{
	struct shape *shapes[3];
	double total_area = 0;
	int i;

	printf("=== vtable_shape_polymorphism demo ===\n\n");

	shapes[0] = circle_create(2.0);
	shapes[1] = rectangle_create(3.0, 4.0);
	shapes[2] = triangle_create(5.0);

	printf("-- Core 遍历异构对象数组，全部走同一套 ops table 调用路径 --\n");
	for (i = 0; i < 3; i++) {
		print_shape_report(shapes[i]);
		total_area += shape_area(shapes[i]);
	}
	printf("\n[core] total area of all shapes = %.3f\n", total_area);

	printf("\n-- 销毁阶段：每个对象自己知道怎么释放自己 --\n");
	for (i = 0; i < 3; i++)
		shape_destroy(shapes[i]);

	printf("\n=== done ===\n");
	return 0;
}
