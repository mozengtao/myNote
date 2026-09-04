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
