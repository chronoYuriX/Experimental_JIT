"""
Marching Squares 核心算法（纯 Python，无 GUI 依赖）
供 test 和 demo 共用
"""
import math

# 16-case 查表：边上交点索引
# 边编号：0=上, 1=右, 2=下, 3=左
EDGE_LUT = [
    [],             # 0000
    [(0, 3)],       # 0001
    [(1, 0)],       # 0010
    [(1, 3)],       # 0011
    [(2, 1)],       # 0100
    [(0, 3), (2, 1)], # 0101 ★鞍点
    [(2, 0)],       # 0110
    [(2, 3)],       # 0111
    [(3, 2)],       # 1000
    [(0, 2)],       # 1001
    [(1, 0), (3, 2)], # 1010 ★鞍点
    [(1, 2)],       # 1011
    [(3, 1)],       # 1100
    [(0, 1)],       # 1101
    [(3, 0)],       # 1110
    [],             # 1111
]

def resolve_ambiguous(case, grid_vals, center_val):
    if case == 5:
        tl, tr, br, bl = grid_vals
        if center_val * tl > 0 or center_val * br > 0:
            return [(0, 1), (2, 3)]
        else:
            return [(0, 3), (2, 1)]
    elif case == 10:
        tl, tr, br, bl = grid_vals
        if center_val * tr > 0 or center_val * bl > 0:
            return [(0, 3), (2, 1)]
        else:
            return [(0, 1), (2, 3)]
    return None

def interpolate(p1, p2, v1, v2, iso=0.0):
    if abs(v2 - v1) < 1e-12:
        return ((p1[0]+p2[0])/2, (p1[1]+p2[1])/2)
    t = (iso - v1) / (v2 - v1)
    t = max(0.0, min(1.0, t))
    return (p1[0]+t*(p2[0]-p1[0]), p1[1]+t*(p2[1]-p1[1]))

def marching_squares(func, xmin, xmax, ymin, ymax, nx, ny):
    """返回 (grid, segments)"""
    grid = []
    for j in range(ny + 1):
        row = []
        y = ymin + (ymax - ymin) * j / ny
        for i in range(nx + 1):
            x = xmin + (xmax - xmin) * i / nx
            try:
                row.append(func(x, y))
            except Exception:
                row.append(float('inf'))
        grid.append(row)

    segments = []
    for j in range(ny):
        for i in range(nx):
            tl, tr = grid[j][i], grid[j][i+1]
            br, bl = grid[j+1][i+1], grid[j+1][i]
            x0 = xmin + (xmax-xmin)*i/nx
            x1 = xmin + (xmax-xmin)*(i+1)/nx
            y0 = ymin + (ymax-ymin)*j/ny
            y1 = ymin + (ymax-ymin)*(j+1)/ny

            case = 0
            if tl >= 0: case |= 8
            if tr >= 0: case |= 4
            if br >= 0: case |= 2
            if bl >= 0: case |= 1
            if case == 0 or case == 15:
                continue

            pts = {}
            if (case & 8) != (case & 4):
                pts[0] = interpolate((x0,y0),(x1,y0), tl, tr)
            if (case & 4) != (case & 2):
                pts[1] = interpolate((x1,y0),(x1,y1), tr, br)
            if (case & 2) != (case & 1):
                pts[2] = interpolate((x1,y1),(x0,y1), br, bl)
            if (case & 1) != (case & 8):
                pts[3] = interpolate((x0,y1),(x0,y0), bl, tl)

            if case in (5, 10):
                cv = (tl+tr+br+bl)/4.0
                segs = resolve_ambiguous(case, [tl,tr,br,bl], cv)
            else:
                segs = EDGE_LUT[case]

            for a, b in segs:
                if a in pts and b in pts:
                    segments.append((pts[a][0], pts[a][1], pts[b][0], pts[b][1]))
    return grid, segments
