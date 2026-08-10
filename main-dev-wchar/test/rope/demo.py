class RopeNode:
    def __init__(self, value):
        self.value = value  # 叶子节点存储字符串，内部节点存储 None
        self.left = None
        self.right = None
        self.weight = len(value) if isinstance(value, str) else 0

class Rope:
    def __init__(self, s=""):
        self.root = RopeNode(s) if s else None

    def _weight(self, node):
        return node.weight if node else 0

    def _concat(self, left, right):
        """合并两个 Rope"""
        if not left:
            return right
        if not right:
            return left
        new_node = RopeNode(None)
        new_node.left = left
        new_node.right = right
        new_node.weight = self._weight(left)
        return new_node

    def insert(self, index, text):
        """在指定位置插入字符串"""
        if not self.root:
            self.root = RopeNode(text)
            return
        left, right = self._split(self.root, index)
        middle = RopeNode(text)
        self.root = self._concat(self._concat(left, middle), right)

    def append(self, text):
        """追加字符串到末尾"""
        new_node = RopeNode(text)
        self.root = self._concat(self.root, new_node)

    def _split(self, node, index):
        """在 index 处分割 rope，返回 (left, right)"""
        if not node:
            return None, None
        if isinstance(node.value, str):  # 叶子节点
            if index >= len(node.value):
                return node, None
            left_str = node.value[:index]
            right_str = node.value[index:]
            return RopeNode(left_str), RopeNode(right_str)
        # 内部节点
        if index < node.weight:
            left_left, left_right = self._split(node.left, index)
            return left_left, self._concat(left_right, node.right)
        else:
            right_left, right_right = self._split(node.right, index - node.weight)
            return self._concat(node.left, right_left), right_right

    def _traverse(self, node):
        """顺序遍历所有叶子节点"""
        result = []
        if node:
            if isinstance(node.value, str):
                result.append(node.value)
            else:
                result.extend(self._traverse(node.left))
                result.extend(self._traverse(node.right))
        return result

    def get_string(self):
        """获取完整字符串"""
        return ''.join(self._traverse(self.root))

    def get_char(self, index):
        """获取指定位置的字符"""
        node = self.root
        while node and not isinstance(node.value, str):
            if index < node.weight:
                node = node.left
            else:
                index -= node.weight
                node = node.right
        if node:
            return node.value[index]
        raise IndexError("Index out of range")

# 使用示例
rope = Rope("Hello")
rope.append(" World!")
print(rope.get_string())  # Hello World!

rope.insert(5, ",")
print(rope.get_string())  # Hello, World!
rope.insert(5, "|1|")
rope.insert(5, "|2|")
rope.insert(6, "|3|")
print(rope.get_string())  # Hello, World!
