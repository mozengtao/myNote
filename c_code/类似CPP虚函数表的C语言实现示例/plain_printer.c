#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "plain_printer.h"

void plain_printer_print(struct plain_printer *self, const char *str);

static const struct printer_i printer_interface = {
	.print = (printer_print_fn)plain_printer_print,
};

struct plain_printer*
plain_printer_new(const char *prefix)
{
	struct plain_printer *self;
	self = malloc(sizeof(struct plain_printer));
	assert(self != NULL);

	self->interface = &printer_interface;
	self->prefix = prefix;

	return self;
}

void
plain_printer_cleanup(struct plain_printer *self)
{
	free(self);
}

void
plain_printer_print(struct plain_printer *self, const char *str)
{
	printf("%s%s", self->prefix, str);
}