CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu11
BINS    = boot_param_setup_sim pci_fixup_table_sim section_based_plugin_table

.PHONY: all clean run

all: $(BINS)

boot_param_setup_sim: boot_param_setup_sim.c
	$(CC) $(CFLAGS) -o $@ $<

pci_fixup_table_sim: pci_fixup_table_sim.c
	$(CC) $(CFLAGS) -o $@ $<

section_based_plugin_table: section_based_plugin_table.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "==== boot_param_setup_sim ===="; ./boot_param_setup_sim
	@echo; echo "==== pci_fixup_table_sim ===="; ./pci_fixup_table_sim
	@echo; echo "==== section_based_plugin_table ===="; ./section_based_plugin_table

clean:
	rm -f $(BINS)
