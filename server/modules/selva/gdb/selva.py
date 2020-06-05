import gdb.printing
import selva_hierarchy

gdb.printing.register_pretty_printer(
        gdb.current_objfile(),
        selva_hierarchy.build_pretty_printer())
