#!/bin/bash

# 递归处理目录，生成类似tree的格式并包含文件内容
# 用法: ./tree_with_content.sh [目录路径]

# 设置目标目录
TARGET_DIR="${1:-.}"

# 输出文件（简短名称）
OUTPUT_FILE="output.txt"

# 检查目录是否存在
if [ ! -d "$TARGET_DIR" ]; then
    echo "错误: 目录 '$TARGET_DIR' 不存在"
    exit 1
fi

# 将目标目录转换为绝对路径
TARGET_DIR=$(cd "$TARGET_DIR" && pwd)

# 递归函数
process_directory() {
    local dir="$1"
    local prefix="$2"
    local is_last="$3"
    
    # 获取目录名
    local dirname=$(basename "$dir")
    
    # 打印目录名
    if [ "$prefix" = "" ]; then
        echo "$dirname/" >> "$OUTPUT_FILE"
    else
        if [ "$is_last" = "true" ]; then
            echo "${prefix}└── $dirname/" >> "$OUTPUT_FILE"
            prefix="${prefix}    "
        else
            echo "${prefix}├── $dirname/" >> "$OUTPUT_FILE"
            prefix="${prefix}│   "
        fi
    fi
    
    # 获取所有文件和目录，排序（目录在前）
    local items=()
    local dirs=()
    local files=()
    
    for item in "$dir"/*; do
        if [ -e "$item" ]; then
            # 跳过输出文件本身
            local basename_item=$(basename "$item")
            if [ "$basename_item" = "$OUTPUT_FILE" ]; then
                continue
            fi
            
            if [ -d "$item" ]; then
                dirs+=("$item")
            else
                files+=("$item")
            fi
        fi
    done
    
    # 先排序目录和文件
    IFS=$'\n' dirs=($(sort <<<"${dirs[*]}"))
    IFS=$'\n' files=($(sort <<<"${files[*]}"))
    
    # 合并：目录在前
    items=("${dirs[@]}" "${files[@]}")
    
    local total=${#items[@]}
    local count=0
    
    for item in "${items[@]}"; do
        count=$((count + 1))
        local last="false"
        if [ $count -eq $total ]; then
            last="true"
        fi
        
        local basename_item=$(basename "$item")
        
        if [ -d "$item" ]; then
            # 递归处理子目录
            if [ "$last" = "true" ]; then
                process_directory "$item" "$prefix" "true"
            else
                process_directory "$item" "$prefix" "false"
            fi
        else
            # 处理文件
            if [ "$last" = "true" ]; then
                echo "${prefix}└── $basename_item" >> "$OUTPUT_FILE"
                print_file_content "$item" "$prefix    "
            else
                echo "${prefix}├── $basename_item" >> "$OUTPUT_FILE"
                print_file_content "$item" "$prefix│   "
            fi
        fi
    done
}

# 打印文件内容（跳过二进制文件）
print_file_content() {
    local file="$1"
    local prefix="$2"
    
    # 检查是否为二进制文件
    if file "$file" | grep -q "text"; then
        # 文本文件，打印内容
        echo "${prefix}内容:" >> "$OUTPUT_FILE"
        while IFS= read -r line || [ -n "$line" ]; do
            echo "${prefix}  $line" >> "$OUTPUT_FILE"
        done < "$file"
        echo "" >> "$OUTPUT_FILE"  # 空行分隔
    else
        # 二进制文件，跳过
        echo "${prefix}[二进制文件，跳过内容]" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
    fi
}

# 开始处理
echo "开始处理目录: $TARGET_DIR"
echo "输出文件: $OUTPUT_FILE"
echo ""

# 清空或创建输出文件
> "$OUTPUT_FILE"

# 执行递归
process_directory "$TARGET_DIR" "" "true"

echo "处理完成！结果保存在: $OUTPUT_FILE"