#ifndef _PRINTER_H_
#define _PRINTER_H_

typedef void(*printer_print_fn)(void *printer, const char *str);

struct printer_i {
	printer_print_fn print;
};

#endif