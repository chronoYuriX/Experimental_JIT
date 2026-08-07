"""
Marching Squares 隐函数绘制演示（Tkinter 版）
============================================
用动画方式逐步展示 MS 算法的每一步：
  1. 网格采样
  2. 每个方格的 case 判定
  3. 边上插值求交点
  4. 连线得到最终曲线

核心算法在 ms_core.py（纯 Python，无 GUI 依赖）。
本文件只负责 GUI、动画调度和绘制。

运行： python3 marching_squares_demo.py
"""

import math, colorsys, sys, os

# 允许直接运行：把同目录加入 path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ms_core import marching_squares, EDGE_LUT, resolve_ambiguous, interpolate

# 延迟导入 tkinter（沙盒无显示器时也能 import 本模块做单元测试）
import tkinter as tk
from tkinter import ttk, messagebox

# ============================================================
# 隐函数库：f(x, y) = 0
# ============================================================
def f_circle(x, y):
    return x * x + y * y - 1.0

def f_heart(x, y):
    return (x * x + y * y - 1.0) ** 3 - x * x * y ** 3 * 0.5

def f_folium(x, y):
    return x ** 3 + y ** 3 - 3 * x * y

def f_lemniscate(x, y):
    r2 = x * x + y * y
    return r2 * r2 - 2.0 * (x * x - y * y) - 0.5

def f_astroid(x, y):
    return (abs(x) ** (2/3) + abs(y) ** (2/3)) - 1.0

def f_spiral(x, y):
    r = math.sqrt(x * x + y * y)
    if r < 0.01: return 1.0
    theta = math.atan2(y, x)
    return math.sin(theta * 3 - r * 3)

def f_sin(x, y):
    return math.sin(x) - y

FUNCTIONS = {
    "圆 x²+y²=1":          f_circle,
    "心形线":              f_heart,
    "叶形线 x³+y³=3xy":    f_folium,
    "双纽线 (含鞍点)":     f_lemniscate,
    "星形线":              f_astroid,
    "螺线":                f_spiral,
    "LOL":                f_sin
}

# ============================================================
# 演示 GUI
# ============================================================
class MSDemoApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Marching Squares 隐函数绘制演示")
        self.root.geometry("1100x800")

        # 参数
        self.func_name = tk.StringVar(value="圆 x²+y²=1")
        self.nx = tk.IntVar(value=20)
        self.ny = tk.IntVar(value=20)
        self.speed = tk.DoubleVar(value=8.0)  # 每格毫秒
        self.show_grid = tk.BooleanVar(value=True)
        self.show_samples = tk.BooleanVar(value=False)
        self.show_cell_cases = tk.BooleanVar(value=False)

        self.xmin, self.xmax = -2.5, 2.5
        self.ymin, self.ymax = -2.5, 2.5

        self.cw, self.ch = 720, 720
        self.pad = 35

        # 状态
        self.grid_data = None
        self.segments = []
        self.cell_queue = []
        self.animating = False
        self.current_step = 0
        self.iso_value = 0.0  # 等值（默认 0）

        self._build_ui()

    # ---------- UI ----------
    def _build_ui(self):
        # 工具栏
        tb = ttk.Frame(self.root, padding=6)
        tb.pack(side=tk.TOP, fill=tk.X)

        ttk.Label(tb, text="函数:").pack(side=tk.LEFT)
        ttk.Combobox(tb, textvariable=self.func_name,
                     values=list(FUNCTIONS.keys()), width=22,
                     state="readonly").pack(side=tk.LEFT, padx=4)

        ttk.Label(tb, text="网格 N:").pack(side=tk.LEFT, padx=(12,0))
        ttk.Entry(tb, textvariable=self.nx, width=5).pack(side=tk.LEFT)
        ttk.Label(tb, text="×").pack(side=tk.LEFT)
        ttk.Entry(tb, textvariable=self.ny, width=5).pack(side=tk.LEFT)

        ttk.Label(tb, text="速度(ms/格):").pack(side=tk.LEFT, padx=(12,0))
        ttk.Entry(tb, textvariable=self.speed, width=6).pack(side=tk.LEFT)

        ttk.Button(tb, text="▶ 开始演示", command=self.start_demo).pack(side=tk.LEFT, padx=8)
        ttk.Button(tb, text="⏹ 停止", command=self.stop_demo).pack(side=tk.LEFT)
        ttk.Button(tb, text="⚡ 一步到位", command=self.draw_all_instant).pack(side=tk.LEFT, padx=4)
        ttk.Button(tb, text="🗑 清空", command=self.clear_canvas).pack(side=tk.LEFT, padx=4)

        # 选项
        opt = ttk.Frame(self.root, padding=4)
        opt.pack(side=tk.TOP, fill=tk.X)
        ttk.Checkbutton(opt, text="显示网格线", variable=self.show_grid,
                        command=self.redraw_layers).pack(side=tk.LEFT)
        ttk.Checkbutton(opt, text="显示采样点", variable=self.show_samples,
                        command=self.redraw_layers).pack(side=tk.LEFT, padx=8)
        ttk.Checkbutton(opt, text="显示 case 色块", variable=self.show_cell_cases,
                        command=self.redraw_layers).pack(side=tk.LEFT, padx=8)

        # 信息栏
        self.info_var = tk.StringVar(value="选择一个函数并点击 ▶ 开始演示")
        ttk.Label(self.root, textvariable=self.info_var,
                  foreground="#333").pack(side=tk.TOP, anchor=tk.W, padx=8)

        # 画布
        self.canvas = tk.Canvas(self.root, width=self.cw, height=self.ch,
                                bg="#fafcff", relief=tk.SUNKEN, border=2)
        self.canvas.pack(side=tk.TOP, padx=8, pady=6)

        # 图例
        leg = ttk.Frame(self.root, padding=4)
        leg.pack(side=tk.TOP, fill=tk.X)
        ttk.Label(leg, text="图例: ", foreground="#555").pack(side=tk.LEFT)
        self._legend_dot(leg, "#d0d8e8", "f<0")
        self._legend_dot(leg, "#fff0f0", "f>0")
        self._legend_dot(leg, "#e08040", "采样点")
        self._legend_dot(leg, "#2060c0", "等值线段")
        self._legend_dot(leg, "#ffd060", "当前格")
        ttk.Label(leg, text="  | 红=正值 蓝=负值", foreground="#888").pack(side=tk.LEFT, padx=6)

        # 状态栏
        self.status_var = tk.StringVar(value="就绪")
        ttk.Label(self.root, textvariable=self.status_var,
                  foreground="#666", font=("Arial", 9)).pack(side=tk.BOTTOM, anchor=tk.W, padx=8, pady=2)

    def _legend_dot(self, parent, color, text):
        f = tk.Frame(parent)
        f.pack(side=tk.LEFT, padx=2)
        tk.Canvas(f, width=14, height=14, bg=color,
                  highlightthickness=1, highlightbackground="#999").pack(side=tk.LEFT)
        ttk.Label(f, text=text, foreground="#555").pack(side=tk.LEFT, padx=2)

    # ---------- 坐标转换 ----------
    def math_to_canvas(self, x, y):
        cx = self.pad + (x - self.xmin) / (self.xmax - self.xmin) * (self.cw - 2*self.pad)
        cy = self.ch - self.pad - (y - self.ymin) / (self.ymax - self.ymin) * (self.ch - 2*self.pad)
        return cx, cy

    # ---------- 绘制 ----------
    def clear_canvas(self):
        self.canvas.delete("all")
        self._draw_axes()

    def _draw_axes(self):
        c = self.canvas
        c.create_rectangle(self.pad, self.pad, self.cw-self.pad, self.ch-self.pad,
                           outline="#888", width=1, dash=(4,2))
        if self.ymin <= 0 <= self.ymax:
            _, y0 = self.math_to_canvas(0, 0)
            c.create_line(self.pad, y0, self.cw-self.pad, y0, fill="#bbb", width=1)
        if self.xmin <= 0 <= self.xmax:
            x0, _ = self.math_to_canvas(0, 0)
            c.create_line(x0, self.pad, x0, self.ch-self.pad, fill="#bbb", width=1)
        for v in range(-2, 3):
            if v == 0: continue
            if self.xmin <= v <= self.xmax:
                xv, _ = self.math_to_canvas(v, 0)
                c.create_line(xv, self.ch-self.pad-3, xv, self.ch-self.pad+3, fill="#888")
                c.create_text(xv, self.ch-self.pad+14, text=str(v), fill="#666", font=("Arial",8))
            if self.ymin <= v <= self.ymax:
                _, yv = self.math_to_canvas(0, v)
                c.create_line(self.pad-3, yv, self.pad+3, yv, fill="#888")
                c.create_text(self.pad-12, yv, text=str(v), fill="#666", font=("Arial",8))

    def _draw_grid_lines(self):
        c = self.canvas
        nx, ny = self.nx.get(), self.ny.get()
        for i in range(nx+1):
            x = self.xmin + (self.xmax-self.xmin)*i/nx
            xc, _ = self.math_to_canvas(x, 0)
            c.create_line(xc, self.pad, xc, self.ch-self.pad,
                          fill="#e0e6ee", width=1, tags="gridline")
        for j in range(ny+1):
            y = self.ymin + (self.ymax-self.ymin)*j/ny
            _, yc = self.math_to_canvas(0, y)
            c.create_line(self.pad, yc, self.cw-self.pad, yc,
                          fill="#e0e6ee", width=1, tags="gridline")

    def _draw_sample_points(self):
        if self.grid_data is None: return
        c = self.canvas
        maxabs = max(max(abs(v) for v in row if math.isfinite(v)) for row in self.grid_data)
        if maxabs < 1e-9: maxabs = 1.0
        nx, ny = self.nx.get(), self.ny.get()
        for j in range(ny+1):
            for i in range(nx+1):
                x = self.xmin + (self.xmax-self.xmin)*i/nx
                y = self.ymin + (self.ymax-self.ymin)*j/ny
                v = self.grid_data[j][i]
                if not math.isfinite(v): continue
                xc, yc = self.math_to_canvas(x, y)
                r = min(abs(v)/maxabs, 1.0)
                if v >= 0:
                    col = f"#{int(255*(0.6+0.4*r)):02x}{int(220*(1-r)):02x}{int(220*(1-r)):02x}"
                else:
                    col = f"#{int(220*(1-r)):02x}{int(220*(1-r)):02x}{int(255*(0.6+0.4*r)):02x}"
                c.create_oval(xc-1.5, yc-1.5, xc+1.5, yc+1.5,
                              fill=col, outline="", tags="sample")

    def _draw_cell_case_fill(self, i, j, case):
        if case in (0, 15): return
        c = self.canvas
        nx, ny = self.nx.get(), self.ny.get()
        x0 = self.xmin + (self.xmax-self.xmin)*i/nx
        y0 = self.ymin + (self.ymax-self.ymin)*j/ny
        x1 = self.xmin + (self.xmax-self.xmin)*(i+1)/nx
        y1 = self.ymin + (self.ymax-self.ymin)*(j+1)/ny
        cx0, cy0 = self.math_to_canvas(x0, y0)
        cx1, cy1 = self.math_to_canvas(x1, y1)
        hue = (case * 47) % 360
        r, g, b = colorsys.hsv_to_rgb(hue/360, 0.30, 1.0)
        col = f"#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}"
        c.create_rectangle(cx0, cy0, cx1, cy1, fill=col, outline="",
                           stipple="gray25", tags="casefill")

    def _draw_one_cell(self, i, j):
        """绘制第 (i,j) 个方格的等值线段，返回画的段数"""
        c = self.canvas
        nx, ny = self.nx.get(), self.ny.get()
        tl = self.grid_data[j][i]
        tr = self.grid_data[j][i+1]
        br = self.grid_data[j+1][i+1]
        bl = self.grid_data[j+1][i]

        x0 = self.xmin + (self.xmax-self.xmin)*i/nx
        x1 = self.xmin + (self.xmax-self.xmin)*(i+1)/nx
        y0 = self.ymin + (self.ymax-self.ymin)*j/ny
        y1 = self.ymin + (self.ymax-self.ymin)*(j+1)/ny

        case = 0
        if tl >= 0: case |= 8
        if tr >= 0: case |= 4
        if br >= 0: case |= 2
        if bl >= 0: case |= 1
        if case in (0, 15): return 0

        pts = {}
        if (case & 8) != (case & 4):
            pts[0] = interpolate((x0,y0),(x1,y0), tl, tr, self.iso_value)
        if (case & 4) != (case & 2):
            pts[1] = interpolate((x1,y0),(x1,y1), tr, br, self.iso_value)
        if (case & 2) != (case & 1):
            pts[2] = interpolate((x1,y1),(x0,y1), br, bl, self.iso_value)
        if (case & 1) != (case & 8):
            pts[3] = interpolate((x0,y1),(x0,y0), bl, tl, self.iso_value)

        if case in (5, 10):
            cv = (tl+tr+br+bl)/4.0
            segs = resolve_ambiguous(case, [tl,tr,br,bl], cv)
        else:
            segs = EDGE_LUT[case]

        cnt = 0
        for a, b in segs:
            if a in pts and b in pts:
                xa, ya = pts[a]; xb, yb = pts[b]
                xac, yac = self.math_to_canvas(xa, ya)
                xbc, ybc = self.math_to_canvas(xb, yb)
                c.create_line(xac, yac, xbc, ybc, fill="#2060c0", width=2, tags="isoline")
                cnt += 1
        return cnt

    def redraw_layers(self):
        self.canvas.delete("gridline")
        self.canvas.delete("sample")
        self.canvas.delete("casefill")
        if self.show_grid.get():
            self._draw_grid_lines()
        if self.show_samples.get() and self.grid_data is not None:
            self._draw_sample_points()

    # ---------- 演示控制 ----------
    def start_demo(self):
        if self.animating: return
        self.clear_canvas()
        func = FUNCTIONS[self.func_name.get()]
        nx, ny = self.nx.get(), self.ny.get()
        if nx < 2 or ny < 2:
            messagebox.showerror("错误", "网格数至少为 2")
            return

        # 预计算
        self.grid_data, self.segments = marching_squares(
            func, self.xmin, self.xmax, self.ymin, self.ymax, nx, ny)

        # 构建方格队列（带 case）
        self.cell_queue = []
        for j in range(ny):
            for i in range(nx):
                tl = self.grid_data[j][i]
                tr = self.grid_data[j][i+1]
                br = self.grid_data[j+1][i+1]
                bl = self.grid_data[j+1][i]
                case = 0
                if tl >= 0: case |= 8
                if tr >= 0: case |= 4
                if br >= 0: case |= 2
                if bl >= 0: case |= 1
                self.cell_queue.append((i, j, case))

        self.animating = True
        self.current_step = 0
        self.info_var.set(
            f"函数: {self.func_name.get()}  |  网格: {nx}×{ny}  |  线段总数: {len(self.segments)}")
        self.status_var.set("演示运行中…")
        self.redraw_layers()
        self._animate_step()

    def _animate_step(self):
        if not self.animating: return
        if self.current_step >= len(self.cell_queue):
            self.animating = False
            self.status_var.set("✅ 演示完成")
            return

        i, j, case = self.cell_queue[self.current_step]

        if self.show_cell_cases.get() and case not in (0, 15):
            self._draw_cell_case_fill(i, j, case)

        segs = self._draw_one_cell(i, j)

        # 高亮当前格
        nx, ny = self.nx.get(), self.ny.get()
        x0 = self.xmin + (self.xmax-self.xmin)*i/nx
        y0 = self.ymin + (self.ymax-self.ymin)*j/ny
        x1 = self.xmin + (self.xmax-self.xmin)*(i+1)/nx
        y1 = self.ymin + (self.ymax-self.ymin)*(j+1)/ny
        cx0, cy0 = self.math_to_canvas(x0, y0)
        cx1, cy1 = self.math_to_canvas(x1, y1)
        hl = self.canvas.create_rectangle(
            cx0, cy0, cx1, cy1, outline="#e08040", width=1, dash=(2,2), tags="highlight")
        self.root.after(60, lambda h=hl: self.canvas.delete(h))

        self.current_step += 1
        self.status_var.set(f"处理中: 第 {self.current_step}/{len(self.cell_queue)} 格")
        delay = max(int(self.speed.get()), 1)
        self.root.after(delay, self._animate_step)

    def stop_demo(self):
        self.animating = False
        self.status_var.set("已停止")

    def draw_all_instant(self):
        self.stop_demo()
        self.clear_canvas()
        func = FUNCTIONS[self.func_name.get()]
        nx, ny = self.nx.get(), self.ny.get()
        self.grid_data, self.segments = marching_squares(
            func, self.xmin, self.xmax, self.ymin, self.ymax, nx, ny)
        self.redraw_layers()
        for x1, y1, x2, y2 in self.segments:
            cx1, cy1 = self.math_to_canvas(x1, y1)
            cx2, cy2 = self.math_to_canvas(x2, y2)
            self.canvas.create_line(cx1, cy1, cx2, cy2,
                                    fill="#2060c0", width=2, tags="isoline")
        self.info_var.set(
            f"函数: {self.func_name.get()}  |  网格: {nx}×{ny}  |  线段: {len(self.segments)}  ✅")
        self.status_var.set("✅ 已完成")


# ============================================================
# 入口
# ============================================================
if __name__ == "__main__":
    root = tk.Tk()
    app = MSDemoApp(root)
    root.mainloop()
