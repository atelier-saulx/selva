import gdb
import gdb.printing
import string

# Helper functions #############################################################

SelvaModify_HierarchyNode_Type = gdb.lookup_type('SelvaModify_HierarchyNode')

def adjVectorToString(vec):
    list = '{ '
    arr =  vec['vec_data'].dereference().cast(SelvaModify_HierarchyNode_Type.pointer().array(vec['vec_last']))
    for i in range(vec['vec_last']):
        list += str(arr[i].dereference()['id']) + ', '

    return list + '}'

# Pretty printers ##############################################################

class SelvaModify_HierarchyNode_Printer:
    "Print SelvaModify_HierarchyNode"

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
    pp = gdb.printing.RegexpCollectionPrettyPrinter("SelvaModify_HierarchyNode")
    pp.add_printer('SelvaModify_HierarchyNode', '^SelvaModify_HierarchyNode$', SelvaModify_HierarchyNode_Printer)
    return pp
