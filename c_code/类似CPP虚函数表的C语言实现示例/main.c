#include <stdio.h>
#include "plain_printer.h"
#include "color_printer.h"

int main(void)
{
	struct plain_printer *p1;
	struct plain_printer *p2;
	struct color_printer *p3;
	struct color_printer *p4;
	struct printer_i **p;

	p1 = plain_printer_new("---- ");
	p2 = plain_printer_new("**** ");
	p3 = color_printer_new("\033[31;47m");
	p4 = color_printer_new("\033[30;42m");

	p = (struct printer_i **)p1;
	(*p)->print(p, "hello p1\n");

	p = (struct printer_i **)p2;
	(*p)->print(p, "hello p2\n");

	p = (struct printer_i **)p3;
	(*p)->print(p, "hello p3\n");

	p = (struct printer_i **)p4;
	(*p)->print(p, "hello p4\n");

	color_printer_disable_color(p4);
	(*p)->print(p, "hello p4\n");

	color_printer_enable_color(p4);
	(*p)->print(p, "hello p4\n");

	plain_printer_cleanup(p1);
	plain_printer_cleanup(p2);
	color_printer_cleanup(p3);
	color_printer_cleanup(p4);

	return 0;
}