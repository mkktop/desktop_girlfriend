#!/usr/bin/env python3
"""
@brief 构建脚本——多板卡自动编译 + 打包
@note  参照 xiaozhi-esp32 的 release.py 简化适配
       核心功能：
       1. --list-boards --json：扫描板卡 config.json，输出构建矩阵
       2. 编译模式：set-target → 追加 sdkconfig → build → merge-bin → zip
       3. 本地打包：merge-bin + zip 当前 build 产物
"""

import sys
import os
import json
import zipfile
import argparse
import re
from pathlib import Path
from typing import Optional

# 切换到项目根目录
os.chdir(Path(__file__).resolve().parent.parent)

################################################################################
# 通用工具函数
################################################################################

def get_project_version() -> str:
    """从根 CMakeLists.txt 读取 set(PROJECT_VER "x.y.z")，无则返回 "0.1.0" """
    with Path("CMakeLists.txt").open(encoding="utf-8") as f:
        for line in f:
            if line.strip().startswith("set(PROJECT_VER"):
                return line.split('"')[1]
    return "0.1.0"


def get_board_type_from_sdkconfig() -> Optional[str]:
    """从 sdkconfig 读取当前编译的 CONFIG_BOARD_TYPE_xxx"""
    sdkconfig = Path("sdkconfig")
    if not sdkconfig.exists():
        return None
    with sdkconfig.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("CONFIG_BOARD_TYPE_") and line.endswith("=y"):
                return line.split("=")[0]
    return None


def merge_bin() -> None:
    """执行 idf.py merge-bin 合并固件"""
    if os.system("idf.py merge-bin") != 0:
        print("merge-bin failed", file=sys.stderr)
        sys.exit(1)


def zip_bin(name: str, version: str) -> None:
    """将 build/merged-binary.bin 打包到 releases/v{version}_{name}.zip"""
    out_dir = Path("releases")
    out_dir.mkdir(exist_ok=True)
    output_path = out_dir / f"v{version}_{name}.zip"

    if output_path.exists():
        output_path.unlink()

    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write("build/merged-binary.bin", arcname="merged-binary.bin")
    print(f"Packaged: {output_path}")


################################################################################
# 板卡 / 变体相关函数
################################################################################

_BOARDS_DIR = Path("main/boards")


def _collect_variants(config_filename: str = "config.json") -> list[dict[str, str]]:
    """扫描 main/boards/ 下所有板卡的 config.json，收集构建变体

    Returns:
        [{"board": "desktop_girlfriend_V1", "name": "desktop_girlfriend_V1", "full_name": "desktop_girlfriend_V1"}, ...]
    """
    variants: list[dict[str, str]] = []

    for cfg_path in sorted(_BOARDS_DIR.rglob(config_filename)):
        board_dir = cfg_path.parent
        if board_dir.name == "common":
            continue
        board = board_dir.relative_to(_BOARDS_DIR).as_posix()

        try:
            with cfg_path.open(encoding="utf-8") as f:
                cfg = json.load(f)

            for build in cfg.get("builds", []):
                name = build["name"]
                variants.append({
                    "board": board,
                    "name": name,
                    "full_name": name,
                })

        except Exception as e:
            print(f"[ERROR] Failed to parse {cfg_path}: {e}", file=sys.stderr)

    return variants


def _board_dir_exists(board_type: str) -> bool:
    """检查板卡目录是否存在"""
    return (_BOARDS_DIR / board_type).is_dir()


################################################################################
# 编译实现
################################################################################

def release(board_type: str, config_filename: str = "config.json", *, filter_name: Optional[str] = None) -> None:
    """编译并打包指定板卡的变体

    Args:
        board_type: 板卡目录名（如 desktop_girlfriend_V1）
        config_filename: 配置文件名
        filter_name: 只编译匹配此名称的变体
    """
    cfg_path = _BOARDS_DIR / board_type / config_filename
    if not cfg_path.exists():
        print(f"[WARN] {cfg_path} does not exist, skipping {board_type}")
        return

    project_version = get_project_version()
    print(f"Project Version: {project_version} ({cfg_path})")

    with cfg_path.open(encoding="utf-8") as f:
        cfg = json.load(f)
    target = cfg["target"]

    builds = cfg.get("builds", [])
    if filter_name:
        builds = [b for b in builds if b["name"] == filter_name]
        if not builds:
            print(f"[ERROR] Variant '{filter_name}' not found in {board_type}", file=sys.stderr)
            sys.exit(1)

    for build in builds:
        name = build["name"]
        sdkconfig_append = build.get("sdkconfig_append", [])

        output_path = Path("releases") / f"v{project_version}_{name}.zip"
        if output_path.exists():
            print(f"Skipping {name} because {output_path} already exists")
            continue

        print("-" * 80)
        print(f"name: {name}")
        print(f"target: {target}")
        for item in sdkconfig_append:
            print(f"sdkconfig_append: {item}")

        # 清除可能残留的 IDF_TARGET 环境变量
        os.environ.pop("IDF_TARGET", None)

        # 设置目标芯片
        if os.system(f"idf.py set-target {target}") != 0:
            print("set-target failed", file=sys.stderr)
            sys.exit(1)

        # 追加 sdkconfig 配置
        with Path("sdkconfig").open("a", encoding="utf-8") as f:
            f.write("\n")
            f.write("# Appended by release.py\n")
            for item in sdkconfig_append:
                f.write(f"{item}\n")

        # 编译
        if os.system("idf.py build") != 0:
            print("build failed", file=sys.stderr)
            sys.exit(1)

        # 合并固件
        merge_bin()

        # 打包
        zip_bin(name, project_version)


################################################################################
# CLI 入口
################################################################################

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build and package firmware for boards")
    parser.add_argument("board", nargs="?", default=None,
                        help="Board type name or 'all'")
    parser.add_argument("-c", "--config", default="config.json",
                        help="Config filename (default: config.json)")
    parser.add_argument("--list-boards", action="store_true",
                        help="List all supported boards and variants")
    parser.add_argument("--json", action="store_true",
                        help="Output in JSON format (use with --list-boards)")
    parser.add_argument("--name",
                        help="Variant name to compile (filter by build name)")

    args = parser.parse_args()

    # 列表模式
    if args.list_boards:
        variants = _collect_variants(config_filename=args.config)
        if args.json:
            print(json.dumps(variants))
        else:
            for v in variants:
                print(f"  {v['board']}: {v['name']}")
        sys.exit(0)

    # 本地打包模式（无参数）
    if args.board is None:
        merge_bin()
        board_cfg = get_board_type_from_sdkconfig()
        if board_cfg is None:
            print("Failed to detect board type from sdkconfig", file=sys.stderr)
            sys.exit(1)
        # CONFIG_BOARD_TYPE_DESKTOP_GIRLFRIEND_V1 → desktop_girlfriend_V1
        board_name = board_cfg.replace("CONFIG_BOARD_TYPE_", "").lower()
        project_ver = get_project_version()
        zip_bin(board_name, project_ver)
        sys.exit(0)

    # 编译模式
    board_type_input: str = args.board
    name_filter: Optional[str] = args.name

    if board_type_input != "all" and not _board_dir_exists(board_type_input):
        print(f"[ERROR] Board directory not found: main/boards/{board_type_input}", file=sys.stderr)
        sys.exit(1)

    variants_all = _collect_variants(config_filename=args.config)

    # 确定要编译的板卡集合
    target_boards: set[str]
    if board_type_input == "all":
        target_boards = {v["board"] for v in variants_all}
    else:
        target_boards = {board_type_input}

    for bt in sorted(target_boards):
        release(bt, config_filename=args.config,
                filter_name=name_filter if bt == board_type_input else None)
