#!/bin/bash

# ============================================================
# Qt 交叉编译脚本 - 支持动态/静态编译，自动处理依赖
# 用法: 在项目根目录下运行 ./qt_build.sh
# ============================================================

set -e

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }
ask() { echo -e "${BLUE}[?]${NC} $1"; }

# 检查 cmake
info "检查 cmake..."
if ! command -v cmake &> /dev/null; then
    error "cmake 未安装，请先安装: sudo apt install cmake"
    exit 1
fi
info "cmake 版本: $(cmake --version | head -1)"

# 检查 MXE
MXE_ROOT=~/mxe
if [ ! -d "$MXE_ROOT" ]; then
    error "MXE 目录不存在: $MXE_ROOT"
    exit 1
fi
info "找到 MXE 环境: $MXE_ROOT"

# 询问编译类型
echo ""
ask "选择编译版本:"
echo "  1) 静态链接 (Static) - 生成的 exe 独立运行，体积大"
echo "  2) 动态链接 (Shared) - 生成的 exe 需要 dll，体积小"
echo ""
read -p "请输入 1 或 2: " choice

case $choice in
    1)
        BUILD_TYPE="static"
        MXE_TARGET="x86_64-w64-mingw32.static"
        BUILD_DIR="build/win_static"
        ;;
    2)
        BUILD_TYPE="shared"
        MXE_TARGET="x86_64-w64-mingw32.shared"
        BUILD_DIR="build/win_shared"
        ;;
    *)
        error "无效选择，请输入 1 或 2"
        exit 1
        ;;
esac

info "选择: ${BUILD_TYPE} 版本"
info "构建目录: ${BUILD_DIR}"

# 进入项目根目录（脚本所在位置）
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# 如果构建目录存在，询问是否删除
if [ -d "$BUILD_DIR" ]; then
    ask "构建目录 '$BUILD_DIR' 已存在，是否删除并重新构建？ (y/n)"
    read -p "请输入 y 或 n: " rm_choice
    if [ "$rm_choice" = "y" ] || [ "$rm_choice" = "Y" ]; then
        info "删除旧目录..."
        rm -rf "$BUILD_DIR"
    else
        info "使用已有目录 (如需重新构建请删除后重试)"
    fi
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 运行 cmake
info "运行 cmake (目标: ${MXE_TARGET})..."
CMAKE_TOOLCHAIN="$MXE_ROOT/usr/${MXE_TARGET}/share/cmake/mxe-conf.cmake"
if [ ! -f "$CMAKE_TOOLCHAIN" ]; then
    error "找不到 CMake 工具链文件: $CMAKE_TOOLCHAIN"
    exit 1
fi

cmake -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN" ../..

if [ $? -ne 0 ]; then
    error "cmake 配置失败"
    exit 1
fi

# 编译
info "开始编译 (使用 -j4)..."
make -j4

if [ $? -ne 0 ]; then
    error "编译失败"
    exit 1
fi

# 查找生成的 exe 文件（优先找 test.exe，因为 CMakeLists 中项目名为 test）
EXE_FILE=""
if [ -f "test.exe" ]; then
    EXE_FILE="test.exe"
elif [ -f "release/test.exe" ]; then
    EXE_FILE="release/test.exe"
else
    # 尝试找任意 .exe
    EXE_FILE=$(find . -maxdepth 2 -name "*.exe" -type f | head -1)
fi

if [ -z "$EXE_FILE" ] || [ ! -f "$EXE_FILE" ]; then
    error "找不到生成的 .exe 文件"
    exit 1
fi

# 获取 exe 文件名（不含路径）
EXE_BASENAME=$(basename "$EXE_FILE")
info "生成的可执行文件: $EXE_FILE"

# ------------------------------------------------------------
# 静态版本：直接留在构建目录，不额外复制
# ------------------------------------------------------------
if [ "$BUILD_TYPE" = "static" ]; then
    info "静态链接版本，生成的 exe 可独立运行"
    echo ""
    info "可执行文件: $SCRIPT_DIR/$BUILD_DIR/$EXE_BASENAME"
    info "测试运行: wine $SCRIPT_DIR/$BUILD_DIR/$EXE_BASENAME"
fi

# ------------------------------------------------------------
# 动态版本：处理依赖到 deploy 子目录
# ------------------------------------------------------------
if [ "$BUILD_TYPE" = "shared" ]; then
    info "动态链接版本，正在处理 DLL 依赖..."

    DEPLOY_DIR="deploy"
    mkdir -p "$DEPLOY_DIR"

    # 复制 exe 到 deploy 目录（仅此一份）
    cp "$EXE_FILE" "$DEPLOY_DIR/"
    info "已复制 $EXE_BASENAME 到 $DEPLOY_DIR/"

    # 使用 copydlldeps.sh 复制依赖
    if [ -f "$MXE_ROOT/tools/copydlldeps.sh" ]; then
        info "运行 copydlldeps.sh 处理依赖..."
        $MXE_ROOT/tools/copydlldeps.sh \
            --infile "$EXE_FILE" \
            --destdir "$DEPLOY_DIR" \
            --recursivesrcdir "$MXE_ROOT/usr/${MXE_TARGET}" \
            --srcdir . \
            --enforcedir "$MXE_ROOT/usr/${MXE_TARGET}/qt6/plugins/platforms/" \
            --objdump "$MXE_ROOT/usr/bin/${MXE_TARGET}-objdump" \
            > /dev/null 2>&1

        # 确保 exe 在 deploy 目录（copydlldeps 可能不会复制 exe）
        if [ ! -f "$DEPLOY_DIR/$EXE_BASENAME" ]; then
            cp "$EXE_FILE" "$DEPLOY_DIR/"
        fi

        info "依赖处理完成，所有文件在 $DEPLOY_DIR/ 目录"
        info "部署目录大小: $(du -sh $DEPLOY_DIR | cut -f1)"
    else
        warn "找不到 copydlldeps.sh，请手动处理依赖"
    fi

    echo ""
    info "部署目录: $SCRIPT_DIR/$BUILD_DIR/$DEPLOY_DIR/"
    info "测试运行: wine $DEPLOY_DIR/$EXE_BASENAME"
    info "将 $DEPLOY_DIR/ 文件夹复制到 Windows 即可运行"
fi

# 汇总
echo ""
echo "============================================================"
info "构建完成！"
echo "  构建类型: ${BUILD_TYPE}"
echo "  构建目录: $SCRIPT_DIR/$BUILD_DIR"
if [ "$BUILD_TYPE" = "shared" ]; then
    echo "  部署目录: $SCRIPT_DIR/$BUILD_DIR/$DEPLOY_DIR"
else
    echo "  可执行文件: $SCRIPT_DIR/$BUILD_DIR/$EXE_BASENAME"
fi
echo "============================================================"
