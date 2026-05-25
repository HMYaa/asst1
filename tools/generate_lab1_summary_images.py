#!/usr/bin/env python3
from __future__ import annotations

import math
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
W, H = 1672, 941

BLUE = "#083b83"
DARK = "#0b1736"
MID = "#195aa6"
LIGHT = "#eaf3ff"
LINE = "#8fb8e8"
GOLD = "#fff6df"
GOLD_LINE = "#f0c66a"
RED = "#d8441f"
GREEN = "#2d8a57"
GRAY = "#536075"
PALE = "#f8fbff"


def fc_match(name: str) -> str:
    out = subprocess.check_output(["fc-match", "-f", "%{file}", name], text=True)
    return out.strip()


FONT_CJK = fc_match("Noto Sans CJK SC")
FONT_LATIN = fc_match("DejaVu Sans")


def font(size: int, bold: bool = False, latin: bool = False) -> ImageFont.FreeTypeFont:
    # NotoSansCJK has readable Chinese glyphs. DejaVu is used sparingly for equations.
    path = FONT_LATIN if latin else FONT_CJK
    return ImageFont.truetype(path, size=size)


@dataclass
class Box:
    x: int
    y: int
    w: int
    h: int

    @property
    def right(self) -> int:
        return self.x + self.w

    @property
    def bottom(self) -> int:
        return self.y + self.h

    def inset(self, dx: int, dy: int) -> "Box":
        return Box(self.x + dx, self.y + dy, self.w - 2 * dx, self.h - 2 * dy)


class Poster:
    def __init__(self, title: str, program: str):
        self.img = Image.new("RGB", (W, H), "white")
        self.draw = ImageDraw.Draw(self.img)
        self.title = title
        self.program = program
        self.draw_title()

    def draw_title(self) -> None:
        badge = Box(W - 250, 15, 232, 76)
        size = 52
        max_w = badge.x - 218
        while size > 36 and self.draw.textlength(self.title, font=font(size)) > max_w:
            size -= 2
        self.draw.text((198, 16), self.title, fill="#06245b", font=font(size))
        self.rounded(
            badge,
            fill="#0a438d",
            outline="#0a438d",
            radius=12,
        )
        self.center_text(Box(W - 250, 19, 232, 68), f"CS149 Assignment 1\n{self.program}", "white", 22)

    def rounded(self, b: Box, fill: str = "white", outline: str = LINE, radius: int = 8, width: int = 2) -> None:
        self.draw.rounded_rectangle((b.x, b.y, b.right, b.bottom), radius=radius, fill=fill, outline=outline, width=width)

    def panel(self, b: Box, n: int, title: str) -> Box:
        self.rounded(b, fill="white", outline="#7aa6dc", radius=8, width=2)
        tag = Box(b.x + 14, b.y + 14, 36, 36)
        self.rounded(tag, fill="#06489a", outline="#06489a", radius=6)
        self.center_text(tag, str(n), "white", 24)
        self.draw.text((b.x + 62, b.y + 14), title, fill=BLUE, font=font(25))
        return Box(b.x + 18, b.y + 64, b.w - 36, b.h - 80)

    def center_text(self, b: Box, text: str, fill: str, size: int, spacing: int = 4) -> None:
        f = font(size)
        lines = text.split("\n")
        heights = [self.draw.textbbox((0, 0), line, font=f)[3] for line in lines]
        total = sum(heights) + spacing * (len(lines) - 1)
        y = b.y + (b.h - total) / 2
        for i, line in enumerate(lines):
            tw = self.draw.textlength(line, font=f)
            self.draw.text((b.x + (b.w - tw) / 2, y), line, fill=fill, font=f)
            y += heights[i] + spacing

    def wrap(self, text: str, f: ImageFont.FreeTypeFont, max_w: int) -> list[str]:
        lines: list[str] = []
        for raw in text.split("\n"):
            if not raw:
                lines.append("")
                continue
            cur = ""
            tokens = re.findall(r"[A-Za-z0-9_./%+\-<>=()[\]`]+|\s+|.", raw)
            for token in tokens:
                if token.isspace() and not cur:
                    continue
                trial = cur + token
                if cur and self.draw.textlength(trial, font=f) > max_w:
                    lines.append(cur.rstrip())
                    cur = "" if token.isspace() else token.lstrip()
                else:
                    cur = trial.rstrip() if token.isspace() else trial
                while cur and self.draw.textlength(cur, font=f) > max_w:
                    cut = max(1, len(cur) - 1)
                    while cut > 1 and self.draw.textlength(cur[:cut], font=f) > max_w:
                        cut -= 1
                    lines.append(cur[:cut].rstrip())
                    cur = cur[cut:].lstrip()
            if cur:
                lines.append(cur)
        return lines

    def text(self, xy: tuple[int, int], text: str, size: int = 22, fill: str = DARK, max_w: int | None = None, leading: int = 8) -> int:
        f = font(size)
        lines = self.wrap(text, f, max_w) if max_w else text.split("\n")
        x, y = xy
        for line in lines:
            self.draw.text((x, y), line, fill=fill, font=f)
            y += size + leading
        return y

    def bullet(self, x: int, y: int, text: str, size: int = 21, max_w: int = 410, fill: str = DARK, dot: str = BLUE) -> int:
        self.draw.ellipse((x, y + 9, x + 9, y + 18), fill=dot)
        return self.text((x + 22, y), text, size=size, fill=fill, max_w=max_w, leading=7)

    def formula(self, b: Box, text: str, size: int = 24, fill: str = LIGHT, outline: str = "#73a9ea") -> None:
        self.rounded(b, fill=fill, outline=outline, radius=7, width=2)
        use_size = size
        lines = text.split("\n")
        while use_size > 14:
            f = font(use_size)
            if all(self.draw.textlength(line, font=f) <= b.w - 22 for line in lines):
                break
            use_size -= 1
        self.center_text(b, text, BLUE, use_size)

    def simple_icon(self, kind: str, b: Box, color: str = BLUE) -> None:
        d = self.draw
        if kind == "target":
            cx, cy = b.x + b.w // 2, b.y + b.h // 2
            for r in (31, 19, 8):
                d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=color, width=4)
            d.line((cx - 42, cy, cx + 42, cy), fill=color, width=4)
            d.line((cx, cy - 42, cx, cy + 42), fill=color, width=4)
        elif kind == "lanes":
            for i in range(4):
                d.rounded_rectangle((b.x, b.y + i * 18, b.right, b.y + i * 18 + 10), radius=5, fill=color)
        elif kind == "tasks":
            for i, dx in enumerate((0, 34, 68)):
                d.ellipse((b.x + dx, b.y + 8, b.x + dx + 24, b.y + 32), fill=color)
                d.rounded_rectangle((b.x + dx - 5, b.y + 34, b.x + dx + 29, b.y + 66), radius=8, fill=color)
        elif kind == "warn":
            pts = [(b.x + b.w // 2, b.y), (b.right, b.bottom), (b.x, b.bottom)]
            d.polygon(pts, outline=RED, fill="#fff1eb")
            d.line((b.x + b.w // 2, b.y + 18, b.x + b.w // 2, b.bottom - 22), fill=RED, width=6)
            d.ellipse((b.x + b.w // 2 - 3, b.bottom - 14, b.x + b.w // 2 + 3, b.bottom - 8), fill=RED)
        elif kind == "check":
            d.rounded_rectangle((b.x + 10, b.y + 5, b.right - 10, b.bottom - 5), radius=8, outline=color, width=4)
            for i in range(3):
                yy = b.y + 19 + i * 18
                d.line((b.x + 22, yy, b.x + 30, yy + 8, b.x + 44, yy - 8), fill=color, width=4)
                d.line((b.x + 56, yy, b.right - 24, yy), fill=color, width=3)
        elif kind == "memory":
            for i in range(3):
                y = b.y + i * 21
                d.rounded_rectangle((b.x, y, b.right, y + 16), radius=8, outline=color, width=4)
        elif kind == "clock":
            d.ellipse((b.x + 8, b.y + 8, b.right - 8, b.bottom - 8), outline=color, width=5)
            cx, cy = b.x + b.w // 2, b.y + b.h // 2
            d.line((cx, cy, cx, b.y + 23), fill=color, width=4)
            d.line((cx, cy, b.right - 23, cy + 15), fill=color, width=4)

    def table(self, b: Box, headers: list[str], rows: list[list[str]], col_fracs: list[float], size: int = 18) -> None:
        self.rounded(b, fill=PALE, outline=LINE, radius=6, width=2)
        xs = [b.x]
        for frac in col_fracs[:-1]:
            xs.append(xs[-1] + int(b.w * frac))
        xs.append(b.right)
        row_h = b.h // (len(rows) + 1)
        self.draw.rectangle((b.x, b.y, b.right, b.y + row_h), fill=LIGHT)
        for x in xs[1:-1]:
            self.draw.line((x, b.y, x, b.bottom), fill=LINE, width=1)
        for r in range(len(rows) + 1):
            y = b.y + r * row_h
            self.draw.line((b.x, y, b.right, y), fill=LINE, width=1)
        for i, h in enumerate(headers):
            self.center_text(Box(xs[i], b.y, xs[i + 1] - xs[i], row_h), h, BLUE, size)
        for r, row in enumerate(rows):
            for c, val in enumerate(row):
                self.center_text(Box(xs[c], b.y + (r + 1) * row_h, xs[c + 1] - xs[c], row_h), val, DARK, size)

    def footer_lessons(self, b: Box, items: list[tuple[str, str, str]]) -> None:
        self.rounded(b, fill="white", outline="#7aa6dc", radius=8, width=2)
        tag = Box(b.x + 14, b.y + 14, 36, 36)
        self.rounded(tag, fill="#06489a", outline="#06489a", radius=6)
        self.center_text(tag, "7", "white", 24)
        self.draw.text((b.x + 62, b.y + 14), "复习抓手", fill=BLUE, font=font(25))
        content = Box(b.x + 35, b.y + 62, b.w - 70, b.h - 76)
        n = len(items)
        col_w = content.w // n
        for i, (icon, title, body) in enumerate(items):
            x = content.x + i * col_w
            if i:
                self.draw.line((x - 15, content.y + 2, x - 15, content.bottom - 2), fill="#b8c7d9", width=1)
            self.simple_icon(icon, Box(x, content.y + 2, 66, 60), BLUE if icon != "warn" else RED)
            text_x = x + (112 if icon == "tasks" else 84)
            self.draw.text((text_x, content.y + 2), title, fill=BLUE, font=font(19))
            self.text((text_x, content.y + 30), body, size=15, fill=DARK, max_w=col_w - (text_x - x) - 18, leading=4)

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        self.img.save(path)


def grid_boxes() -> tuple[list[Box], Box]:
    margin, gap = 16, 0
    top_y, panel_h = 100, 365
    mid_y, mid_h = 464, 330
    col_w = (W - 2 * margin - 2 * gap) // 3
    top = [Box(margin + i * (col_w + gap), top_y, col_w, panel_h) for i in range(3)]
    mid = [Box(margin + i * (col_w + gap), mid_y, col_w, mid_h) for i in range(3)]
    return top + mid, Box(margin, 800, W - 2 * margin, 124)


def draw_row_bars(p: Poster, b: Box, interleaved: bool = False) -> None:
    colors = ["#1e5da8", "#2f8fbe", "#e3a324", "#d45b3b"]
    rows = 12
    for r in range(rows):
        y = b.y + r * (b.h // rows)
        h = b.h // rows - 3
        if interleaved:
            c = colors[r % 4]
        else:
            c = colors[min(3, r // 3)]
        cost = 0.35 + 0.55 * math.exp(-((r - rows * 0.55) ** 2) / 7)
        p.draw.rounded_rectangle((b.x, y, b.x + int(b.w * cost), y + h), radius=4, fill=c)


def prog1() -> Poster:
    p = Poster("Prog1 Mandelbrot Threads：负载均衡决定加速比", "Program 1")
    boxes, footer = grid_boxes()

    c = p.panel(boxes[0], 1, "目标：线程级并行")
    p.simple_icon("target", Box(c.x + 4, c.y + 24, 88, 88))
    p.formula(Box(c.x + 126, c.y + 6, 335, 58), "每个像素独立计算 mandel(x, y)", 22)
    y = c.y + 88
    y = p.bullet(c.x + 126, y, "使用 std::thread 并行生成 Mandelbrot 图像", max_w=360)
    y = p.bullet(c.x + 126, y + 4, "比较 block 与 interleaved 两种静态划分", max_w=360)
    p.bullet(c.x + 126, y + 4, "不共享写，无需互斥锁；最后仍有 join 等待点", max_w=360)

    c = p.panel(boxes[1], 2, "性能模型：最慢线程决定总时间")
    p.formula(Box(c.x + 30, c.y + 12, c.w - 60, 70), "T_parallel ≈ max(T_thread_i) + spawn/join", 23)
    p.simple_icon("clock", Box(c.x + 20, c.y + 110, 72, 72))
    y = c.y + 104
    y = p.bullet(c.x + 116, y, "Mandelbrot 边界附近迭代多，行代价不均", max_w=385)
    y = p.bullet(c.x + 116, y + 2, "join 等待所有 worker，straggler 会拖住整体", max_w=385)
    p.bullet(c.x + 116, y + 2, "加速比非线性不是算法不可并行，而是负载分配不均", max_w=385)

    c = p.panel(boxes[2], 3, "两种划分：连续块 vs 行交错")
    left = Box(c.x + 20, c.y + 18, 215, 185)
    right = Box(c.x + 285, c.y + 18, 215, 185)
    p.draw.text((left.x, left.y - 2), "block：连续行块", fill=BLUE, font=font(20))
    p.draw.text((right.x, right.y - 2), "interleaved：轮转行", fill=BLUE, font=font(20))
    draw_row_bars(p, Box(left.x, left.y + 36, left.w, 130), False)
    draw_row_bars(p, Box(right.x, right.y + 36, right.w, 130), True)
    p.text((c.x + 20, c.y + 222), "block 简单但可能把重行集中给同一线程；interleaved 把重/轻行打散，仍然是静态、无通信的方案。", 20, max_w=c.w - 38)

    c = p.panel(boxes[3], 4, "实现骨架")
    p.formula(Box(c.x + 16, c.y + 4, c.w - 32, 54), "创建 N-1 个 std::thread + 主线程 worker0  →  join", 21)
    y = c.y + 82
    y = p.bullet(c.x + 18, y, "`computeBlockRange()` 计算 [startRow, endRow)", max_w=470)
    y = p.bullet(c.x + 18, y + 2, "`workerThreadStart_Interleaved()` 按 j += numThreads 扫行", max_w=470)
    y = p.bullet(c.x + 18, y + 2, "`-p` 打印 per-thread 时间，用数据定位 straggler", max_w=470)
    p.bullet(c.x + 18, y + 2, "`mandelbrotThread()` 默认选择 interleaved", max_w=470)

    c = p.panel(boxes[4], 5, "实验现象：为什么 8 线程接近上限")
    p.table(
        Box(c.x + 18, c.y + 4, c.w - 36, 154),
        ["现象", "解释"],
        [
            ["block 波动", "重行集中造成拖尾"],
            ["interleaved 更稳", "行代价被打散"],
            ["-p 计时", "直接定位慢线程"],
            ["view 变化", "行代价分布会变化"],
            ["16 线程平台", "硬件线程/调度竞争"],
        ],
        [0.36, 0.64],
        18,
    )
    p.text((c.x + 22, c.y + 190), "myth 机器约 4 个物理核 / 8 个硬件线程；8 线程附近通常接近平台，16 线程收益有限或下降。", 19, max_w=c.w - 40)

    c = p.panel(boxes[5], 6, "答题关键词")
    y = c.y + 10
    for s in [
        "像素独立：正确性上易并行",
        "代价不均：性能上难负载均衡",
        "本实现静态划分：不引入动态任务队列/抢任务同步",
        "straggler：总时间看最慢线程",
        "测量闭环：speedup 曲线 + per-thread time",
    ]:
        y = p.bullet(c.x + 25, y, s, max_w=455)

    p.footer_lessons(
        footer,
        [
            ("check", "先判断依赖", "无共享写冲突；但 join 是等待点。"),
            ("clock", "看最慢者", "并行时间由最后完成的 worker 决定。"),
            ("lanes", "静态打散负载", "interleaved 用简单规则降低拖尾。"),
            ("warn", "别迷信线程数", "硬件线程多不等于线性加速。"),
        ],
    )
    return p


def prog2() -> Poster:
    p = Poster("Prog2 SIMD Intrinsics：mask、lane 和指标取舍", "Program 2")
    boxes, footer = grid_boxes()

    c = p.panel(boxes[0], 1, "目标：向量化 clampedExp")
    p.simple_icon("lanes", Box(c.x + 18, c.y + 38, 80, 92))
    p.formula(Box(c.x + 128, c.y + 10, 340, 64), "output[i] = min(values[i]^exp[i], 9.999999)", 20)
    y = c.y + 94
    y = p.bullet(c.x + 128, y, "用 CS149 intrinsics 一条指令处理 W 个 lane", max_w=350)
    y = p.bullet(c.x + 128, y + 2, "exponent 每个 lane 不同，需要 mask 表达分歧", max_w=350)
    p.bullet(c.x + 128, y + 2, "N 不整除 W 时，尾部必须用 partial mask", max_w=350)

    c = p.panel(boxes[1], 2, "SIMD 执行模型")
    p.formula(Box(c.x + 22, c.y + 4, c.w - 44, 58), "一条向量指令 = 同一 opcode + 多个 lane", 22)
    y = c.y + 88
    y = p.bullet(c.x + 20, y, "每条 intrinsic 的 mask=1 lane 会计入 utilized lanes", max_w=475)
    y = p.bullet(c.x + 20, y + 4, "mask=0 不是分支跳过整条指令，而是关闭对应 lane", max_w=475)
    p.bullet(c.x + 20, y + 4, "`cntbits` 用来判断是否还有活跃 lane", max_w=475)

    c = p.panel(boxes[2], 3, "三版 clampedExp 的取舍")
    p.table(
        Box(c.x + 14, c.y + 2, c.w - 28, 188),
        ["版本", "指令/利用率", "核心做法"],
        [
            ["v1", "97075 / 80.7%", "动态 while + cntbits"],
            ["v2", "95003 / 66.9%", "固定 9 次循环检查"],
            ["v3", "95003 / 66.9%", "精简 count 初始化"],
        ],
        [0.16, 0.35, 0.49],
        17,
    )
    p.text((c.x + 18, c.y + 208), "优化目标主要是减少 Total Vector Instructions；Utilization 用来诊断 lane 空转。", 20, max_w=c.w - 34)

    c = p.panel(boxes[3], 4, "Utilization 为什么会下降")
    p.formula(Box(c.x + 18, c.y + 2, c.w - 36, 56), "Utilization = 活跃 lane 数 / 总 lane 数", 22)
    y = c.y + 82
    y = p.bullet(c.x + 18, y, "固定 9 次循环检查后，很多 lane 已算完但循环仍继续", max_w=470)
    y = p.bullet(c.x + 18, y + 2, "后几轮 maskActive 很稀疏，拉低平均利用率", max_w=470)
    y = p.bullet(c.x + 18, y + 2, "有效乘法仍由 count>0 的 mask 决定", max_w=470)
    y = p.bullet(c.x + 18, y + 2, "但少了 cntbits，Total Vector Instructions 反而更少", max_w=470)

    c = p.panel(boxes[4], 5, "尾部与 arraySum 加分")
    p.formula(Box(c.x + 14, c.y + 2, c.w - 28, 54), "maskAll = ones(min(W, N - i))", 22)
    y = c.y + 80
    y = p.bullet(c.x + 18, y, "尾部若不用 rem mask，会越界 load/store 或污染输出", max_w=470)
    y = p.bullet(c.x + 18, y + 2, "`arraySumVector` 假设 N % VECTOR_WIDTH == 0", size=19, max_w=470)
    y = p.bullet(c.x + 18, y + 2, "分块累加后，用 `hadd` + `interleave` 做树形归约", size=19, max_w=470)
    p.bullet(c.x + 18, y + 2, "目标复杂度约 O(N/W + log W)", size=19, max_w=470)

    c = p.panel(boxes[5], 6, "调试方式")
    p.simple_icon("check", Box(c.x + 18, c.y + 12, 82, 82))
    y = c.y + 2
    y = p.bullet(c.x + 122, y, "`./myexp -s 3` 专门测尾部 partial vector", max_w=360)
    y = p.bullet(c.x + 122, y + 2, "`./myexp -s 10000` 看指令数和利用率", max_w=360)
    y = p.bullet(c.x + 122, y + 2, "`./myexp -l` 看每条指令的 lane 图案", max_w=360)
    p.bullet(c.x + 122, y + 2, "修改 VECTOR_WIDTH 后要 clean rebuild", max_w=360)

    p.footer_lessons(
        footer,
        [
            ("lanes", "mask 是核心", "分歧控制流靠 mask 表达。"),
            ("warn", "指标会冲突", "利用率高不等于总指令少。"),
            ("check", "尾部必测", "N % W != 0 是常见正确性坑。"),
            ("target", "归约有结构", "水平求和要按树形移动部分和。"),
        ],
    )
    return p


def prog3() -> Poster:
    p = Poster("Prog3 ISPC Mandelbrot：foreach + launch 叠加并行", "Program 3")
    boxes, footer = grid_boxes()

    c = p.panel(boxes[0], 1, "目标：同一问题，两层并行")
    p.formula(Box(c.x + 20, c.y + 6, c.w - 40, 56), "Mandelbrot：像素独立，但每个像素迭代次数不同", 21)
    y = c.y + 88
    y = p.bullet(c.x + 20, y, "`mandelbrot_ispc`：主要利用单核 SIMD", max_w=470)
    y = p.bullet(c.x + 20, y + 3, "`mandelbrot_ispc_withtasks`：多核 task + task 内 SIMD", max_w=470)
    p.bullet(c.x + 20, y + 3, "目标是解释为何无 tasks 低于 8x，并让 tasks 版本超过 32x", max_w=470)

    c = p.panel(boxes[1], 2, "ISPC 执行模型")
    p.table(
        Box(c.x + 18, c.y + 4, c.w - 36, 178),
        ["概念", "含义"],
        [
            ["gang", "一组 SIMD program instances"],
            ["programCount", "本题 AVX2 i32x8，即 8"],
            ["programIndex", "当前 program instance 编号"],
            ["uniform/varying", "全 lane 相同 / 每 lane 不同"],
        ],
        [0.36, 0.64],
        17,
    )
    p.text((c.x + 20, c.y + 204), "`foreach` 描述独立迭代；program instance 可近似理解为 gang 内 lane 位置。", 20, max_w=c.w - 40)

    c = p.panel(boxes[2], 3, "为何 SIMD 低于理论 8x")
    p.simple_icon("lanes", Box(c.x + 18, c.y + 26, 86, 90))
    y = c.y + 4
    y = p.bullet(c.x + 126, y, "`mandel()` 内部有 per-lane 早退", max_w=360)
    y = p.bullet(c.x + 126, y + 2, "同一 gang 中有些 lane 结束，有些还在迭代", max_w=360)
    y = p.bullet(c.x + 126, y + 2, "ISPC 用 mask 关闭已完成 lane，循环等最慢 lane", max_w=360)
    p.bullet(c.x + 126, y + 2, "view 2 边界更复杂，divergence 更严重", max_w=360)

    c = p.panel(boxes[3], 4, "tasks：把核间并行显式交给 launch")
    p.formula(Box(c.x + 16, c.y + 4, c.w - 32, 54), "launch[numTasks] task(...)  +  task 内 foreach", 21)
    y = c.y + 82
    y = p.bullet(c.x + 20, y, "starter 只有 2 tasks，最多约用 2 个核", size=19, max_w=470)
    y = p.bullet(c.x + 20, y + 2, "16/32 tasks：占满多核，并把连续行块切小", size=19, max_w=470)
    y = p.bullet(c.x + 20, y + 2, "运行时调度多个小块，降低 straggler", size=19, max_w=470)
    y = p.bullet(c.x + 20, y + 2, "launch 维度需 uniform；本实现固定为 32", size=19, max_w=470)
    p.bullet(c.x + 20, y + 2, "通用写法要把最后一个 yend clamp 到 height", size=19, max_w=470)

    c = p.panel(boxes[4], 5, "并行层次图")
    p.draw.text((c.x + 20, c.y + 4), "无 tasks：调用线程顺序执行多个 gang", fill=BLUE, font=font(19))
    p.draw.text((c.x + 20, c.y + 142), "有 tasks：多个 gang 分配到多个 core", fill=BLUE, font=font(19))
    for i in range(4):
        y = c.y + 34 + i * 24
        color = BLUE if i == 0 else "#c9d5e4"
        p.draw.rounded_rectangle((c.x + 40, y, c.x + 146, y + 15), radius=5, fill=color)
        p.draw.text((c.x + 155, y - 3), f"Core{i}", fill=DARK, font=font(15))
    for i in range(4):
        y = c.y + 174 + i * 24
        p.draw.rounded_rectangle((c.x + 40, y, c.x + 146, y + 15), radius=5, fill=BLUE)
        p.draw.text((c.x + 155, y - 3), f"Core{i}", fill=DARK, font=font(15))
    p.text((c.x + 282, c.y + 50), "foreach 管单核 SIMD\nlaunch 管多核 task\n4 核 × 8 lane 只是粗略模型", 18, max_w=215)

    c = p.panel(boxes[5], 6, "与 Prog1 / Prog2 的联系")
    p.table(
        Box(c.x + 14, c.y + 6, c.w - 28, 220),
        ["题目", "并行单元", "主要瓶颈"],
        [
            ["Prog1", "std::thread", "线程间负载不均"],
            ["Prog2", "SIMD lane", "mask / lane 空转"],
            ["Prog3", "ISPC gang + task", "两类瓶颈叠加"],
        ],
        [0.22, 0.38, 0.40],
        17,
    )
    p.text((c.x + 18, c.y + 244), "同一问题换并行抽象，瓶颈表达方式也会变。", 18, max_w=c.w - 34)

    p.footer_lessons(
        footer,
        [
            ("lanes", "foreach 是 SIMD", "主要利用调用线程所在 core 的 SIMD。"),
            ("tasks", "launch 是多核", "task 数决定核间并行粒度。"),
            ("clock", "仍要防拖尾", "行块太大时重任务会拖慢整体。"),
            ("warn", "硬件模型不是硬承诺", "实测可因机器和调度高于或低于粗略模型。"),
        ],
    )
    return p


def prog4() -> Poster:
    p = Poster("Prog4 Iterative sqrt：同组最慢 lane 拖住 SIMD", "Program 4")
    boxes, footer = grid_boxes()

    c = p.panel(boxes[0], 1, "目标：用输入构造观察 SIMD 边界")
    p.formula(Box(c.x + 18, c.y + 4, c.w - 36, 60), "while (pred > threshold) 反复迭代逼近 sqrt", 21)
    y = c.y + 90
    y = p.bullet(c.x + 20, y, "报告 random 输入下 ISPC 与 task ISPC 加速比", max_w=470)
    y = p.bullet(c.x + 20, y + 3, "构造 best 输入，让 SIMD 加速尽量大", max_w=470)
    p.bullet(c.x + 20, y + 3, "构造 worst 输入，让无 tasks ISPC 尽量慢", max_w=470)

    c = p.panel(boxes[1], 2, "SPMD on SIMD")
    p.simple_icon("lanes", Box(c.x + 18, c.y + 28, 88, 90))
    y = c.y + 6
    y = p.bullet(c.x + 126, y, "ISPC 写起来像每个元素有独立 while", max_w=360)
    y = p.bullet(c.x + 126, y + 2, "硬件上同一 gang 仍共享 SIMD 指令流", max_w=360)
    y = p.bullet(c.x + 126, y + 2, "已收敛 lane 被 mask 掉，但循环继续等待慢 lane", max_w=360)
    p.bullet(c.x + 126, y + 2, "关键是同组 8 个元素迭代次数是否接近", max_w=360)

    c = p.panel(boxes[2], 3, "第一性原理模型")
    p.formula(Box(c.x + 18, c.y + 6, c.w - 36, 52), "T_serial ≈ Σ iter[i]", 24)
    p.formula(Box(c.x + 18, c.y + 72, c.w - 36, 52), "T_simd ≈ Σ max(iter[group of 8])", 23)
    p.formula(Box(c.x + 18, c.y + 138, c.w - 36, 52), "util ≈ Σiter / (8 × Σmax(group))", 21)
    p.text((c.x + 20, c.y + 206), "串行看总迭代数；SIMD 看组内最大迭代数。一个慢 lane 会让 7 个快 lane 空转。", 19, max_w=c.w - 40)

    c = p.panel(boxes[3], 4, "输入设计")
    p.table(
        Box(c.x + 14, c.y + 4, c.w - 28, 204),
        ["模式", "values[i]", "目的"],
        [
            ["random", "[0.001, 2.999]", "真实基线"],
            ["best", "全部 2.999", "同慢，高利用率"],
            ["worst", "i%8==0: 2.999; else 1.0", "一慢七快"],
        ],
        [0.22, 0.43, 0.35],
        15,
    )
    p.text((c.x + 18, c.y + 232), "注意：i%8 针对 programCount=8；3.0f 从 guess=1 会不收敛。", 17, fill=RED, max_w=c.w - 34)

    c = p.panel(boxes[4], 5, "实验结果如何读")
    p.table(
        Box(c.x + 18, c.y + 4, c.w - 36, 176),
        ["模式", "无 tasks", "task ISPC", "含义"],
        [
            ["random", "本机约 3.66x", "本机约 70x", "有收益但有分歧"],
            ["best", "本机约 5.52x", "本机约 125x", "lane 同步更好"],
            ["worst", "本机约 0.76x", "本机约 15x", "SIMD 被慢 lane 拖住"],
        ],
        [0.22, 0.23, 0.24, 0.31],
        15,
    )
    p.text((c.x + 20, c.y + 202), "数字依赖机器；复习应记趋势和模型。best 的关键是每个 lane 工作量相似。", 19, max_w=c.w - 40)

    c = p.panel(boxes[5], 6, "tasks 能做什么，不能做什么")
    p.simple_icon("tasks", Box(c.x + 18, c.y + 18, 100, 88))
    y = c.y + 0
    y = p.bullet(c.x + 140, y, "源码用 64 tasks，让多个 CPU core 同时处理不同数据块", max_w=340)
    y = p.bullet(c.x + 140, y + 2, "它不能消除单个 gang 内部的 lane 分歧", max_w=340)
    y = p.bullet(c.x + 140, y + 2, "worst 下仍比 best 差，说明瓶颈没有消失", max_w=340)
    p.bullet(c.x + 140, y + 2, "多核收益和 SIMD 利用率是两个层次", max_w=340)

    p.footer_lessons(
        footer,
        [
            ("lanes", "SIMD 不等于 8x", "宽度是上界，利用率才决定收益。"),
            ("clock", "组内看最大值", "一个慢 lane 会拖住整个 gang。"),
            ("target", "输入能隔离变量", "best/worst 是性能模型的验证实验。"),
            ("warn", "tasks 不治 lane 分歧", "它只是在更多 core 上执行同样的 SIMD 模型。"),
        ],
    )
    return p


POSTERS: list[tuple[Callable[[], Poster], Path]] = [
    (prog1, ROOT / "prog1_mandelbrot_threads" / "prog1_mandelbrot_threads_summary.png"),
    (prog2, ROOT / "prog2_vecintrin" / "prog2_vecintrin_summary.png"),
    (prog3, ROOT / "prog3_mandelbrot_ispc" / "prog3_mandelbrot_ispc_summary.png"),
    (prog4, ROOT / "prog4_sqrt" / "prog4_sqrt_summary.png"),
]


def main() -> None:
    for build, out in POSTERS:
        poster = build()
        poster.save(out)
        print(out)


if __name__ == "__main__":
    main()
