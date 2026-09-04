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
