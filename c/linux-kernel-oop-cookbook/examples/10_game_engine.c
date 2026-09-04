/*
 * 10_game_engine.c - Game engine entities: NPC / Monster / Player
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct entity;

struct entity_ops {
	void (*update)(struct entity *self, float dt);
	void (*render)(struct entity *self);
	void (*on_hit)(struct entity *self, int damage);
	void (*destroy)(struct entity *self);
};

struct entity {
	const char *name;
	float x, y;
	int hp;
	const struct entity_ops *ops;
};

static void entity_update(struct entity *self, float dt)
{
	if (self && self->ops && self->ops->update)
		self->ops->update(self, dt);
}

static void entity_render(struct entity *self)
{
	if (self && self->ops && self->ops->render)
		self->ops->render(self);
}

static void entity_hit(struct entity *self, int damage)
{
	if (self && self->ops && self->ops->on_hit)
		self->ops->on_hit(self, damage);
}

static void entity_destroy(struct entity *self)
{
	if (self && self->ops && self->ops->destroy)
		self->ops->destroy(self);
}

/* Player */
struct player {
	struct entity base;
	int score;
	int mana;
};

static void player_update(struct entity *self, float dt)
{
	struct player *p = container_of(self, struct player, base);

	self->x += 10.0f * dt;
	p->mana += 1;
	printf("[Player] %s update pos=(%.1f,%.1f) mana=%d score=%d\n",
	       self->name, self->x, self->y, p->mana, p->score);
}

static void player_render(struct entity *self)
{
	struct player *p = container_of(self, struct player, base);

	printf("[Player] RENDER %s hp=%d score=%d\n",
	       self->name, self->hp, p->score);
}

static void player_on_hit(struct entity *self, int damage)
{
	struct player *p = container_of(self, struct player, base);

	self->hp -= damage;
	printf("[Player] %s hit -%d hp=%d\n", self->name, damage, self->hp);
	if (self->hp <= 0)
		printf("[Player] %s DIED (score=%d)\n", self->name, p->score);
}

static void player_destroy(struct entity *self)
{
	free(container_of(self, struct player, base));
}

static const struct entity_ops player_ops = {
	.update  = player_update,
	.render  = player_render,
	.on_hit  = player_on_hit,
	.destroy = player_destroy,
};

static struct entity *player_create(const char *name, float x, float y)
{
	struct player *p = calloc(1, sizeof(*p));

	if (!p)
		return NULL;
	p->base.name = name;
	p->base.x = x;
	p->base.y = y;
	p->base.hp = 100;
	p->base.ops = &player_ops;
	p->score = 0;
	p->mana = 50;
	return &p->base;
}

/* NPC */
struct npc {
	struct entity base;
	const char *dialog;
};

static void npc_update(struct entity *self, float dt)
{
	(void)dt;
	printf("[NPC] %s idles at (%.1f,%.1f)\n",
	       self->name, self->x, self->y);
}

static void npc_render(struct entity *self)
{
	struct npc *n = container_of(self, struct npc, base);

	printf("[NPC] RENDER %s says \"%s\"\n", self->name, n->dialog);
}

static void npc_on_hit(struct entity *self, int damage)
{
	(void)damage;
	printf("[NPC] %s is friendly - hit ignored\n", self->name);
}

static void npc_destroy(struct entity *self)
{
	free(container_of(self, struct npc, base));
}

static const struct entity_ops npc_ops = {
	.update  = npc_update,
	.render  = npc_render,
	.on_hit  = npc_on_hit,
	.destroy = npc_destroy,
};

static struct entity *npc_create(const char *name, float x, float y,
				 const char *dialog)
{
	struct npc *n = calloc(1, sizeof(*n));

	if (!n)
		return NULL;
	n->base.name = name;
	n->base.x = x;
	n->base.y = y;
	n->base.hp = 1;
	n->base.ops = &npc_ops;
	n->dialog = dialog;
	return &n->base;
}

/* Monster */
struct monster {
	struct entity base;
	int attack_power;
	float aggro_range;
};

static void monster_update(struct entity *self, float dt)
{
	struct monster *m = container_of(self, struct monster, base);

	self->x -= 5.0f * dt;
	printf("[Monster] %s hunts pos=(%.1f,%.1f) atk=%d aggro=%.1f\n",
	       self->name, self->x, self->y, m->attack_power, m->aggro_range);
}

static void monster_render(struct entity *self)
{
	struct monster *m = container_of(self, struct monster, base);

	printf("[Monster] RENDER %s hp=%d atk=%d\n",
	       self->name, self->hp, m->attack_power);
}

static void monster_on_hit(struct entity *self, int damage)
{
	self->hp -= damage;
	printf("[Monster] %s hit -%d hp=%d\n", self->name, damage, self->hp);
	if (self->hp <= 0)
		printf("[Monster] %s SLAIN\n", self->name);
}

static void monster_destroy(struct entity *self)
{
	free(container_of(self, struct monster, base));
}

static const struct entity_ops monster_ops = {
	.update  = monster_update,
	.render  = monster_render,
	.on_hit  = monster_on_hit,
	.destroy = monster_destroy,
};

static struct entity *monster_create(const char *name, float x, float y,
				     int atk, float aggro)
{
	struct monster *m = calloc(1, sizeof(*m));

	if (!m)
		return NULL;
	m->base.name = name;
	m->base.x = x;
	m->base.y = y;
	m->base.hp = 40;
	m->base.ops = &monster_ops;
	m->attack_power = atk;
	m->aggro_range = aggro;
	return &m->base;
}

/* Engine tick */
static void world_tick(struct entity **ents, int n, float dt)
{
	int i;

	printf("--- tick dt=%.2f ---\n", dt);
	for (i = 0; i < n; i++)
		entity_update(ents[i], dt);
	for (i = 0; i < n; i++)
		entity_render(ents[i]);
}

int main(void)
{
	struct entity *world[3];
	int i;

	printf("=== 10 Game Engine ===\n");
	world[0] = player_create("Hero", 0.0f, 0.0f);
	world[1] = npc_create("Villager", 5.0f, 0.0f, "Welcome, traveler!");
	world[2] = monster_create("Goblin", 20.0f, 0.0f, 8, 12.0f);

	world_tick(world, 3, 1.0f);

	printf("--- combat ---\n");
	entity_hit(world[2], 15);
	entity_hit(world[0], 5);
	entity_hit(world[1], 1);
	entity_hit(world[2], 30);

	printf("--- cleanup ---\n");
	for (i = 0; i < 3; i++)
		entity_destroy(world[i]);
	printf("Done.\n");
	return 0;
}
