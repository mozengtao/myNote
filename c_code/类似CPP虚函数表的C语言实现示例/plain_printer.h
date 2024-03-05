#ifndef _PLAIN_PRINTER_H_
#define _PLAIN_PRINTER_H_

#include "printer.h"

struct plain_printer {
	const struct printer_i *interface;
	const char *prefix;
};


struct plain_printer *plain_printer_new(const char *prefix);
void plain_printer_cleanup(struct plain_printer *self);

#endif