import tkinter

class UIclass:
    def __init__(self, size, color):
        self.size = size
        self.top = tkinter.Tk()
        self.top.geometry("%dx%d" % (size[0] + 2, size[1] + 2))
        self.top.title("Marching squares demo")
        self.canvas = tkinter.Canvas(
            self.top, width = size[0], height = size[1], bg = color)
        self.canvas.place(x = 0, y = 0)
    def line(self, line, color):
        self.canvas.create_line(line[0] + (self.size[0] / 2), line[1] + (self.size[1] / 2), line[2] + (self.size[0] / 2), line[3] + (self.size[1] / 2), fill = color, width = 2)
    def clear(self):
        self.canvas.delete(tkinter.ALL)
    def update(self):
        self.canvas.update()

def draw_axis(UI, range_x, range_y, step, magnify):
    range_x_copy = [range_x[0], range_x[1]]
    while range_x_copy[0] < range_x_copy[1]:
        UI.line((range_x_copy[0] * magnify, range_y[0] * magnify, range_x_copy[0] * magnify, range_y[1] * magnify), "#003300")
        range_x_copy[0] += step
    range_y_copy = [range_y[0], range_y[1]]
    while range_y_copy[0] < range_y_copy[1]:
        UI.line((range_x[0] * magnify, range_y_copy[0] * magnify, range_x[1] * magnify, range_y_copy[0] * magnify), "#003300")
        range_y_copy[0] += step

# O--------> x
# | [0] [1]
# | [2] [3]
# y
march_map = {
    ( True,  True, False, False): (0.0, 0.5, 1.0, 0.5), (False, False,  True,  True): (0.0, 0.5, 1.0, 0.5),
    ( True, False,  True, False): (0.5, 0.0, 0.5, 1.0), (False,  True, False,  True): (0.5, 0.0, 0.5, 1.0),
    # [0] 左下-右上对角线
    (False,  True,  True,  True): (0.0, 0.5, 0.5, 0.0), 
    ( True, False, False, False): (0.0, 0.5, 0.5, 0.0),  # 这是原来缺失的对称case
    # [1] 右下-左上对角线
    (False,  True, False, False): (0.5, 0.0, 1.0, 0.5), 
    ( True, False,  True,  True): (0.5, 0.0, 1.0, 0.5),
    # [2] 左下-右上对角线
    (False, False,  True, False): (0.0, 0.5, 0.5, 1.0), 
    ( True,  True, False,  True): (0.0, 0.5, 0.5, 1.0),
    # [3] 右下-左上对角线
    (False, False, False,  True): (0.5, 1.0, 1.0, 0.5), 
    ( True,  True,  True, False): (0.5, 1.0, 1.0, 0.5)   # 这里不再被覆盖
}

def _2_screen(current, march, step, magnify):
    return (current + march * step) * magnify
def _9_grid(UI, func, start, end, _4_vals, magnify, max_depth = 6, depth = 0):
    mid = ((start[0] + end[0]) / 2.0, (start[1] + end[1]) / 2.0)
    line = march_map.get(_4_vals, None)
    step = mid[0] - start[0]
    if line != None:
        if depth == max_depth:
            todraw = (_2_screen(current_x, line[0], step, magnify), _2_screen(current_y, line[1], step, magnify),
                      _2_screen(current_x, line[2], step, magnify), _2_screen(current_y, line[3], step, magnify))
            UI.line(todraw, "00FF00")
        else:
            _9_grid(start, mid)
            _9_grid(mid  , end)
            _9_grid((  mid[0], start[1]), (end[0], mid[1]))
            _9_grid((start[0],   mid[1]), (mid[1], end[1]))
def draw_func(UI, func, start, end, step, magnify, max_depth = 6, depth = 0, cache = None):
    if cache is None:
        cache = {}
    val_map = []
    y = start[1]
    while y <= end[1]:
        x = start[0]
        val_map.append([])
        while x <= end[0]:
            key = (round(x, 5), round(y, 5))  # 用坐标作为缓存键，四舍五入避免浮点误差
            if key not in cache:
                cache[key] = float(func(x, y))
            val_map[-1].append(cache[key])
            x += step
        y += step
    current_y = start[1]
    for i in range(len(val_map) - 1):
        current_x = start[0]
        for j in range(len(val_map[0]) - 1):
            key = (val_map[i][j] > 0, val_map[i][j + 1] > 0, val_map[i + 1][j] > 0, val_map[i + 1][j + 1] > 0)
            line = march_map.get(key, None)
            if line != None:
                if depth == max_depth:
                    todraw = [current_x + line[0] * step, current_y + line[1] * step, current_x + line[2] * step, current_y + line[3] * step]
                    for k in range(len(todraw)):
                        todraw[k] *= magnify
                    UI.line(todraw, "#00FF00")
                else:
                    draw_func(UI, func, [current_x, current_y], [current_x + step, current_y + step], step / 2, magnify, max_depth, depth + 1, cache)
            current_x += step
        current_y += step
    if depth == 0:
        UI.update()


from math import sin, cos
from time import sleep
UI = UIclass((1080, 720), "#000000")
##r = 0
##while r < 10.0:
##    UI.clear()
##    draw_axis(UI, [-50.0, 50.0], [-40.0, 40.0], 1.0, 10.0)
##    draw_func(UI, lambda x, y: x ** 3 - y ** 3 + 3 * r * x * y, [-20.0, -20.0], [20.0, 20.0], 0.5, 20.0)
##    sleep(0.2)
##    r += 0.5
draw_axis(UI, [-55.0, 55.0], [-45.0, 45.0], 1.0, 25.0)
draw_func(UI, lambda x, y: y * sin(x) - x * cos(y) - 1, [-20.0, -10.0], [20.0, 10.0], 2.0, 50.0)
