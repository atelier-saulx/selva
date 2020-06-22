python
import sys
sys.path.insert(0, 'gdb')
gdb.execute('add-symbol-file module.so')
import selva
end
