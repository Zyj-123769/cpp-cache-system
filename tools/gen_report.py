#!/usr/bin/env python3
"""从基准程序的实际输出生成「自包含」HTML 报告，避免手写数值随代码变更而漂移。

用法：
    python tools/gen_report.py [--build-dir build] [--out <path>]

前置：先构建出 bench / compare_lru_lfu / compare_arc / compare_arc_phases / bench_sharded
（或直接用 `cmake --build build --target report`，由 CMake 负责依赖与调用）。
生成的 HTML 不依赖任何外部资源（字体 / JS 都用系统回退），可直接打开。
"""
import argparse
import datetime
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUT = os.path.join(ROOT, "cache-benchmark-report", "cache-benchmark-report.html")
BENCH_PROGRAMS = ["bench", "compare_lru_lfu", "compare_arc", "compare_arc_phases", "bench_sharded"]


def find_exe(build_dir, name):
    for cand in (name + ".exe", name):
        path = os.path.join(build_dir, cand)
        if os.path.exists(path):
            return path
    raise SystemExit(f"找不到可执行文件 {name}（请先构建项目：cmake --build build）")


def run_program(build_dir, name):
    return subprocess.run([find_exe(build_dir, name)], capture_output=True,
                          text=True, check=True).stdout


def parse_bench(text):
    """bench 输出：capacity hit_rate% time_ms Mops/s"""
    rows = []
    for line in text.splitlines():
        m = re.match(r"\s*(\d+)\s+([\d.]+)%\s+([\d.]+)\s+([\d.]+)\s*$", line)
        if m:
            rows.append((int(m.group(1)), float(m.group(2)),
                         float(m.group(3)), float(m.group(4))))
    return rows


def rate_after(text, label):
    """抓取 "<label> ... 43.86%" 里的百分比。"""
    m = re.search(re.escape(label) + r"[^\d\n]*([\d.]+)%", text)
    return float(m.group(1)) if m else None


def parse_lru_lfu(text):
    """解析形如 "LRU: 整体命中率 = 0.00%  热点命中率 = 0.00%" 的行。"""
    out = {}
    for cache in ("LRU", "LFU"):
        m = re.search(re.escape(cache) +
                      r":\s*整体命中率\s*=\s*([\d.]+)%.*?热点命中率\s*=\s*([\d.]+)%",
                      text)
        if m:
            out[f"{cache} 整体"] = float(m.group(1))
            out[f"{cache} 热点"] = float(m.group(2))
    return out


def fmt_pct(v):
    return "—" if v is None else f"{v:.2f}%"


def pct_str(v):
    return "—" if v is None else f"{v:.2f}%"


def bars_svg(rows, value_index, color, max_value=None, unit="%"):
    """用内联 SVG 画一组水平条（不依赖任何图表库）。"""
    if not rows:
        return "<p class='muted'>无数据</p>"
    values = [r[value_index] for r in rows]
    top = max_value if max_value else max(values + [1.0])
    bar_h, gap, width = 22, 10, 620
    height = len(rows) * (bar_h + gap)
    parts = [f"<svg viewBox='0 0 {width} {height}' width='100%' "
             f"style='max-width:{width}px' role='img'>"]
    for i, r in enumerate(rows):
        v = r[value_index]
        w = 0 if top == 0 else int(v / top * (width - 120))
        y = i * (bar_h + gap)
        parts.append(
            f"<text x='0' y='{y + 15}' font-size='12' fill='#5b6577'>{r[0]}</text>"
            f"<rect x='48' y='{y}' width='{w}' height='{bar_h}' rx='3' fill='{color}'/>"
            f"<text x='{48 + w + 6}' y='{y + 15}' font-size='12' fill='#1a2233'>"
            f"{v:.2f}{unit}</text>")
    parts.append("</svg>")
    return "".join(parts)


def row_table(rows, headers):
    head = "".join(f"<th>{h}</th>" for h in headers)
    body = ""
    for r in rows:
        body += "<tr>" + "".join(f"<td class='num'>{c}</td>" for c in r) + "</tr>"
    return f"<table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table>"


def parse_sharded(text):
    """解析 bench_sharded 矩阵输出：返回 (线程列表, [(分片数, [吞吐...]), ...])。"""
    threads, rows = [], []
    for line in text.splitlines():
        s = line.strip()
        if "shards" in s and "thr" in s:
            threads = [int(x) for x in re.findall(r"\d+", s)]
            continue
        m = re.match(r"^(\d+)\s+((?:\d+\.\d+\s*)+)$", s)
        if m:
            rows.append((int(m.group(1)), [float(x) for x in m.group(2).split()]))
    return threads, rows


def sharded_table(threads, rows):
    if not rows:
        return "<p class='muted'>无数据</p>"
    head = "".join(f"<th>{t}</th>" for t in threads)
    body = ""
    for shards, vals in rows:
        cells = "".join(f"<td class='num'>{v:.2f}</td>" for v in vals)
        body += f"<tr><td class='num'>{shards}</td>{cells}</tr>"
    return (f"<table><thead><tr><th>分片\\线程</th>{head}</tr></thead>"
            f"<tbody>{body}</tbody></table>")


def build_html(data):
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    bench_rows = data["bench"]
    bench_tbl = [(r[0], f"{r[1]:.2f}%", f"{r[2]:.1f}", f"{r[3]:.1f}") for r in bench_rows]

    lr = data["lru_lfu"]
    lr_tbl = (row_table(
        [("LRU", fmt_pct(lr.get("LRU 整体")), fmt_pct(lr.get("LRU 热点"))),
         ("LFU", fmt_pct(lr.get("LFU 整体")), fmt_pct(lr.get("LFU 热点")))],
        ["缓存", "整体命中率", "热点命中率"]))

    arc = data["arc"]
    arc_tbl = row_table(
        [("LRU", pct_str(arc.get("LRU"))), ("LFU", pct_str(arc.get("LFU"))),
         ("ARC", pct_str(arc.get("ARC")))],
        ["缓存", "命中率"])

    ph = data["arc_phases"]
    ph_tbl = row_table(
        [("LRU", pct_str(ph.get("LRU"))), ("LFU", pct_str(ph.get("LFU"))),
         ("ARC", pct_str(ph.get("ARC")))],
        ["缓存", "命中率"])

    sh_threads, sh_rows = data["sharded"]
    sh_tbl = sharded_table(sh_threads, sh_rows)

    return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>缓存系统基准测试报告 · LRU / LFU / ARC</title>
<style>
:root {{ --bg:#f6f8fb; --card:#fff; --ink:#1a2233; --muted:#5b6577; --rule:#e5e9f0;
         --a1:#2563eb; --a2:#7c3aed; --a3:#059669; }}
* {{ box-sizing:border-box; }}
body {{ margin:0; padding:2rem 1rem; background:var(--bg); color:var(--ink);
        font-family:-apple-system,'Segoe UI','Microsoft YaHei',sans-serif; line-height:1.7; }}
main {{ max-width:960px; margin:0 auto; }}
h1 {{ font-size:2rem; margin:0 0 .3rem; letter-spacing:-.01em; }}
h2 {{ font-size:1.2rem; margin:2.2rem 0 .8rem; }}
.sub {{ color:var(--muted); }}
.card {{ background:var(--card); border:1px solid var(--rule); border-radius:12px;
         padding:1rem 1.2rem; margin:1rem 0; }}
table {{ width:100%; border-collapse:collapse; font-variant-numeric:tabular-nums; }}
th,td {{ padding:.5rem .6rem; border-bottom:1px solid var(--rule); text-align:left; }}
th {{ color:var(--muted); font-weight:600; font-size:.85rem; }}
td.num,th:not(:first-child) {{ text-align:right; }}
.muted {{ color:var(--muted); }}
code {{ background:#eef2f8; padding:.1rem .35rem; border-radius:5px;
        font-family:Consolas,monospace; font-size:.9em; }}
footer {{ margin-top:2.5rem; color:var(--muted); font-size:.85rem;
          border-top:1px solid var(--rule); padding-top:1rem; }}
</style>
</head>
<body>
<main>
  <h1>缓存系统基准测试报告</h1>
  <p class="sub">LRU / LFU / ARC · 自包含报告（由 <code>tools/gen_report.py</code> 从基准程序输出自动生成）</p>

  <h2>1. 容量 - 命中率 / 吞吐（100 万次随机 get/put，zipf 热点）</h2>
  <div class="card">
    {row_table(bench_tbl, ["容量", "命中率", "耗时 ms", "吞吐 Mops/s"])}
    <div style="margin-top:1rem">{bars_svg(bench_rows, 1, "#2563eb")}</div>
    <p class="muted">命中率随容量单调上升；容量 ≥ 热点数时接近 100%。吞吐不随容量衰减，佐证 get/put 为 O(1)。</p>
  </div>

  <h2>2. LRU vs LFU（偶发冷扫描 + 稳定热点）</h2>
  <div class="card">
    {lr_tbl}
    <p class="muted">LFU 抗缓存污染：热点命中率显著高于 LRU。</p>
  </div>

  <h2>3. ARC vs LRU vs LFU（循环扫描 + 局部热点）</h2>
  <div class="card">
    {arc_tbl}
    <p class="muted">ARC 融合近期/频繁局部性，命中率不低于 LRU。</p>
  </div>

  <h2>4. 相位切换负载（热点集合整体切换 + 扫描污染）</h2>
  <div class="card">
    {ph_tbl}
    <p class="muted">LFU 因「频次固化」滞后；ARC 靠指针 p 与 B1/B2 幽灵列表及时跟进新热点。</p>
  </div>

  <h2>5. ShardedLRUCache 并发吞吐（分片数 × 线程数）</h2>
  <div class="card">
    {sh_tbl}
    <p class="muted">单位 Mops/s。数值受本机核数影响，仅示意扩展趋势：分片越多，并发吞吐扩展越好；分片数 = 1（单把锁）在并发下受限。</p>
  </div>

  <footer>
    生成时间：{now} ｜ 复现：<code>cmake --build build --target report</code>
    （或先构建后运行 <code>python tools/gen_report.py</code>）
  </footer>
</main>
</body>
</html>
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=os.path.join(ROOT, "build"))
    ap.add_argument("--out", default=DEFAULT_OUT)
    args = ap.parse_args()

    texts = {name: run_program(args.build_dir, name) for name in BENCH_PROGRAMS}

    lru_lfu = texts["compare_lru_lfu"]
    data = {
        "bench": parse_bench(texts["bench"]),
        "lru_lfu": parse_lru_lfu(lru_lfu),
        "arc": {
            "LRU": rate_after(texts["compare_arc"], "LRU 命中率"),
            "LFU": rate_after(texts["compare_arc"], "LFU 命中率"),
            "ARC": rate_after(texts["compare_arc"], "ARC 命中率"),
        },
        "arc_phases": {
            "LRU": rate_after(texts["compare_arc_phases"], "LRU 命中率"),
            "LFU": rate_after(texts["compare_arc_phases"], "LFU 命中率"),
            "ARC": rate_after(texts["compare_arc_phases"], "ARC 命中率"),
        },
        "sharded": parse_sharded(texts["bench_sharded"]),
    }

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(build_html(data))
    print(f"报告已生成：{args.out}")


if __name__ == "__main__":
    main()
