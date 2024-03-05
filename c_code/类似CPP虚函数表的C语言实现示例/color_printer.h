#ifndef _COLOR_PRINTER_H_
#define _COLOR_PRINTER_H_

#include "printer.h"

struct color_printer {
	const struct printer_i *interface;
	int enable_color;
	const char *color_command;
	char *buf;
};


struct color_printer *color_printer_new(const char *color_command);
void color_printer_cleanup(struct color_printer *self);
void color_printer_disable_color(struct color_printer *self);
void color_printer_enable_color(struct color_printer *self);

#endif