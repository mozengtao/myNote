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
