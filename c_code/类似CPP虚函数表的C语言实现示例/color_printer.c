#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "color_printer.h"

void color_printer_print(struct color_printer *self, const char *str);

static const struct printer_i printer_interface = {
	.print = (printer_print_fn)color_printer_print,
};

struct color_printer *color_printer_new(const char *color_command)
{
	struct color_printer *self;
	self = malloc(sizeof(struct color_printer));
	assert(self != NULL);

	self->interface = &printer_interface;
	self->color_command = color_command == NULL ? "\033[31;40m" : color_command;
	self->enable_color = 1;

	self->buf = malloc(100);
	assert(self->buf != NULL);

	return self;
}

void color_printer_cleanup(struct color_printer *self)
{
	free(self->buf);
	free(self);
}

void color_printer_print(struct color_printer *self, const char *str)
{
	if(self->enable_color) {
		printf("%s%s\033[0m", self->color_command, str);
	} else {
		printf("%s", str);
	}
}

void color_printer_disable_color(struct color_printer *self)
{
	self->enable_color = 0;
}

void color_printer_enable_color(struct color_printer *self)
{
	self->enable_color = 1;
}