import sys
from graphviz import Digraph

trie = None

class tree_node():
    def __init__(self):
        self.val = 256
        self.left = None
        self.right = None
    def insertLeft(self, val):
        if self.left == None:
            self.left = tree_node(val)
        else:
            self.left.val = val

def parse_ip(string):
    ip = [int(i) for i in string.split('/')[0].split('.')]
    port = int(ip[2])
    length = int(string.split('/')[1])

    ip_num = ip[0]*(2**24)+ip[1]*(2**16)+ip[2]*(2**8)+ip[3]
    return ip_num, port, length

def add_node(ip, port, length):
    true_ip = bin(ip)[2:]
    if len(bin(ip)) < 34:
        true_ip = '0'*(34-len(bin(ip))) + bin(ip)[2:]

    now = trie
    for i in range(length):
        if true_ip[i] == '0':
            if now.left == None:
                now.left = tree_node()

            now = now.left
            if i == length - 1:
                now.val = port
        elif true_ip[i] == '1':
            if now.right == None:
                now.right = tree_node()

            now = now.right
            if i == length - 1:
                now.val = port


def draw_child(dot, n, parent_name):
    if n.left != None:
        show_val = str(n.left.val) if n.left.val != 256 else ''
        dot.node(parent_name+'0', show_val)
        dot.edge(parent_name, parent_name+'0', '0')
        draw_child(dot, n.left, parent_name+'0')
    if n.right != None:
        show_val = str(n.right.val) if n.right.val != 256 else ''
        dot.node(parent_name+'1', show_val)
        dot.edge(parent_name, parent_name+'1', '1')
        draw_child(dot, n.right, parent_name+'1')
        

def draw_plot():
    global trie
    dot = Digraph(comment='Binary Trie')
    dot.node(" ", "root")

    draw_child(dot, trie, " ")
    dot.render('./binary_trie.gv', view=True)

def main():
    # read file
    ip_file = open(sys.argv[1], 'r')

    global trie
    trie = tree_node()

    for cnt, line in enumerate(ip_file):
        ip, port, length = parse_ip(line)
        add_node(ip, port, length)

    draw_plot()

if __name__ == "__main__":
    main()
    
