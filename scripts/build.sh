#!/bin/bash

# ==========================================
# 一键编译脚本
# ==========================================
# 用法：
#   ./build.sh          → 编译（Debug 模式）
#   ./build.sh release  → 编译（Release 模式，有优化）
#   ./build.sh clean    → 清理 build 目录
#   ./build.sh rebuild  → 清理后重新编译

set -e  # 任何命令失败立即退出

# 项目根目录（脚本所在目录）
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="${PROJECT_DIR}/build"
BIN_DIR="${BUILD_DIR}/bin"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

# ==========================================
# 处理命令行参数
# ==========================================

BUILD_TYPE="Debug"

case "$1" in
    release|Release)
        BUILD_TYPE="Release"
        ;;
    clean)
        info "Cleaning build directory..."
        rm -rf "${BUILD_DIR}"
        info "Done."
        exit 0
        ;;
    rebuild)
        info "Rebuilding..."
        rm -rf "${BUILD_DIR}"
        # 不 exit，继续往下走去编译
        ;;
    ""|debug|Debug)
        BUILD_TYPE="Debug"
        ;;
    *)
        error "Unknown option: $1"
        echo "Usage: $0 [debug|release|clean|rebuild]"
        exit 1
        ;;
esac

# ==========================================
# 编译
# ==========================================

info "Build type: ${BUILD_TYPE}"
info "Build directory: ${BUILD_DIR}"

# 创建 build 目录
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 运行 CMake
info "Running CMake..."
cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${PROJECT_DIR}"

# 编译（使用所有 CPU 核心）
NPROC=$(nproc 2>/dev/null || echo 2)
info "Compiling with ${NPROC} threads..."
make -j"${NPROC}"

# ==========================================
# 编译完成，列出生成的可执行文件
# ==========================================

echo ""
info "========== Build Successful =========="
info "Executables:"

if [ -d "${BIN_DIR}" ]; then
    for f in "${BIN_DIR}"/*; do
        if [ -x "$f" ] && [ -f "$f" ]; then
            SIZE=$(du -h "$f" | cut -f1)
            echo "  ${GREEN}→${NC} $f  (${SIZE})"
        fi
    done
else
    warn "No bin directory found"
fi

echo ""
info "To run:"
info "  Server:  ${BIN_DIR}/chatserver"
info "  Client:  ${BIN_DIR}/chatclient"
info "  Stress:  ${BIN_DIR}/stress_test 10000"
