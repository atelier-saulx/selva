import gdb
import gdb.printing
import string

# Helper functions #############################################################

SelvaHierarchyNode_Type = gdb.lookup_type('SelvaHierarchyNode')

def adjVectorToString(vec):
    list = '{ '
    arr =  vec['vec_data'].dereference().cast(SelvaHierarchyNode_Type.pointer().array(vec['vec_last']))
    for i in range(vec['vec_last']):
        list += str(arr[i].dereference()['id']) + ', '

    return list + '}'

# Pretty printers ##############################################################

class SelvaHierarchyNode_Printer:
    "Print SelvaHierarchyNode"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        if self.val.address == 0:
            return "NULL"

        node_id = self.val['id']
        node_visit_stamp = self.val['visit_stamp']
        node_parents = self.val['parents']
        node_children = self.val['children']

        node = '{id: '            + str(node_id) + \
            ', visit_stamp: '   + hex(node_visit_stamp['tv_sec'])[2:] + hex(node_visit_stamp['tv_nsec'])[2:] + \
            ', parents: '       + adjVectorToString(node_parents) + \
            ', children: '      + adjVectorToString(node_children) + \
            '}'

        return node

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("SelvaHierarchyNode")
    pp.add_printer('SelvaHierarchyNode', '^SelvaHierarchyNode$', SelvaHierarchyNode_Printer)
    return pp

# Commands #####################################################################

class SelvaModify_Vector_Print(gdb.Command):
    "Print contents of a Vector"

    def __init__(self):
        super(SelvaModify_Vector_Print, self).__init__("print-vector",
                gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)

        try:
            vec = gdb.parse_and_eval(args[0])
            vType = gdb.lookup_type(args[1])
        except:
            gdb.write('No symbol "%s" in current context or invalid type %s.\n' % str(args[0]), str(args[1]))
            return

        arr =  vec['vec_data'].dereference().cast(vType.pointer().array(vec['vec_last']))
        for i in range(vec['vec_last']):
            print(str(i) + ': ' + str(arr[i].dereference()) + '\n')

SelvaModify_Vector_Print()
