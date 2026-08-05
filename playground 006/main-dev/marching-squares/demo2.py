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
        self.canvas.create_line(line[0] + (self.size[0] / 2), line[1] + (self.size[1] / 2), line[2] + (self.size[0] / 2), line[3] + (self.size[1] / 2), fill = color)
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
    ( True,  True, False, False): (0.0, 0.5, 1.0, 0.5), (False, False,  True,  True): (0.0, 0.5, 1.0, 0.5), # ---
    ( True, False,  True, False): (0.5, 0.0, 0.5, 1.0), (False,  True, False,  True): (0.5, 0.0, 0.5, 1.0), #  |
    (False,  True,  True,  True): (0.0, 0.5, 0.5, 0.0), ( True, False, False, False): (0.0, 0.5, 0.5, 0.0), # [0]
    (False,  True, False, False): (0.5, 0.0, 1.0, 0.5), ( True, False,  True,  True): (0.5, 0.0, 1.0, 0.5), # [1]
    (False, False,  True, False): (0.0, 0.5, 0.5, 1.0), ( True,  True, False,  True): (0.0, 0.5, 0.5, 1.0), # [2] 
    (False, False, False,  True): (0.5, 1.0, 1.0, 0.5), ( True,  True,  True, False): (0.5, 1.0, 1.0, 0.5)  # [3]
}

def _2_screen(current, march, step, magnify):
    return (current + march * step) * magnify
def _9_grid(UI, func, start, end, step, _4_vals, magnify, max_depth, depth):
    mid = ((start[0] + end[0]) / 2.0, (start[1] + end[1]) / 2.0)
    march = march_map.get(_4_vals, None)
    if march != None:
        if depth == max_depth:
            UI.line((_2_screen(start[0], march[0], step, magnify), _2_screen(start[1], march[1], step, magnify),
                     _2_screen(start[0], march[2], step, magnify), _2_screen(start[1], march[3], step, magnify)), "#00FF00")
        else:
            _5_vals = (                            float(func(mid[0], start[1])) > 0,                                   # {0} [0] {1}
                float(func(start[0], mid[1])) > 0, float(func(mid[0],   mid[1])) > 0, float(func(end[0], mid[1])) > 0,  # [1] [2] [3]
                                                   float(func(mid[0],   end[1])) > 0                                  ) # {2} [4] {3}
            depth += 1
            step /= 2
            _9_grid(UI, func, start, mid, step, (_4_vals[0], _5_vals[0], _5_vals[1], _5_vals[2]), magnify, max_depth, depth)
            _9_grid(UI, func, mid  , end, step, (_5_vals[2], _5_vals[3], _5_vals[4], _4_vals[3]), magnify, max_depth, depth)
            _9_grid(UI, func, (  mid[0], start[1]), (end[0], mid[1]), step, (_5_vals[0], _4_vals[1], _5_vals[2], _5_vals[3]), magnify, max_depth, depth)
            _9_grid(UI, func, (start[0],   mid[1]), (mid[0], end[1]), step, (_5_vals[1], _5_vals[2], _4_vals[2], _5_vals[4]), magnify, max_depth, depth)
def draw_func(UI, func, start, end, step, magnify, max_depth = 6):
    val_map = []
    y = start[1]
    while y <= end[1]:
        x = start[0]
        val_map.append([])
        while x <= end[0]:
            val_map[-1].append(float(func(x, y)))
            x += step
        y += step
    current_y = start[1]
    for i in range(len(val_map) - 1):
        current_x = start[0]
        for j in range(len(val_map[0]) - 1):
            _9_grid(UI, func,
                    (current_x, current_y),
                    (current_x + step, current_y + step),
                    step,
                    (val_map[i][j] > 0, val_map[i][j + 1] > 0, val_map[i + 1][j] > 0, val_map[i + 1][j + 1] > 0),
                    magnify, max_depth, 0)
            current_x += step
        current_y += step
    UI.update()

from math import sin, cos
UI = UIclass((1080, 720), "#000000")
draw_axis(UI, [-55.0, 55.0], [-45.0, 45.0], 1.0, 25.0)
draw_func(UI, lambda x, y: y * sin(x) - x * cos(y) - 1.0, [-20.0, -10.0], [20.0, 10.0], 2.0, 50.0)
