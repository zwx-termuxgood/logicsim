#!/bin/bash

# 递归处理多个目录/文件，生成类似tree的格式并包含文件内容
# 用法: ./merge.sh [路径1] [路径2] ...
#   示例: ./merge.sh src/ config/ README.md

# 输出文件
OUTPUT_FILE="${OUTPUT_FILE:-output.txt}"

# 至少需要一个参数
if [ $# -eq 0 ]; then
    echo "用法: $0 <目录或文件> [目录或文件] ..."
    exit 1
fi

# 递归函数
process_directory() {
    local dir="$1"
    local prefix="$2"
    local is_last="$3"

    local dir_name
    dir_name=$(basename "$dir")

    # 打印目录名
    if [ -z "$prefix" ]; then
        echo "$dir_name/" >> "$OUTPUT_FILE"
    else
        if [ "$is_last" = "true" ]; then
            echo "${prefix}└── $dir_name/" >> "$OUTPUT_FILE"
            prefix="${prefix}    "
        else
            echo "${prefix}├── $dir_name/" >> "$OUTPUT_FILE"
            prefix="${prefix}│   "
        fi
    fi

    local dirs=()
    local files=()
    local item

    shopt -s nullglob
    for item in "$dir"/*; do
        local bname
        bname=$(basename "$item")
        # 跳过输出文件本身
        if [ "$bname" = "$OUTPUT_FILE" ]; then
            continue
        fi
        if [ -d "$item" ]; then
            dirs+=("$item")
        else
            files+=("$item")
        fi
    done
    shopt -u nullglob

    # 排序（避免空数组产生空元素）
    if [ ${#dirs[@]} -gt 0 ]; then
        IFS=$'\n' dirs=($(printf '%s\n' "${dirs[@]}" | sort))
    fi
    if [ ${#files[@]} -gt 0 ]; then
        IFS=$'\n' files=($(printf '%s\n' "${files[@]}" | sort))
    fi

    local items=("${dirs[@]}" "${files[@]}")
    local total=${#items[@]}
    local count=0

    for item in "${items[@]}"; do
        count=$((count + 1))
        local last="false"
        [ $count -eq $total ] && last="true"

        local bname
        bname=$(basename "$item")

        if [ -d "$item" ]; then
            process_directory "$item" "$prefix" "$last"
        else
            if [ "$last" = "true" ]; then
                echo "${prefix}└── $bname" >> "$OUTPUT_FILE"
                print_file_content "$item" "$prefix    "
            else
                echo "${prefix}├── $bname" >> "$OUTPUT_FILE"
                print_file_content "$item" "$prefix│   "
            fi
        fi
    done
}

# 打印文件内容（跳过二进制文件）
print_file_content() {
    local file="$1"
    local prefix="$2"

    # 用 grep -I 判断是否为二进制：-I 表示忽略二进制文件
    if grep -Iq . "$file" 2>/dev/null || [ -s "$file" ]; then
        # 空文件或文本文件
        if [ ! -s "$file" ]; then
            echo "${prefix}内容: [空文件]" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
            return
        fi
        if grep -Iq . "$file" 2>/dev/null; then
            echo "${prefix}内容:" >> "$OUTPUT_FILE"
            while IFS= read -r line || [ -n "$line" ]; do
                echo "${prefix}  $line" >> "$OUTPUT_FILE"
            done < "$file"
            echo "" >> "$OUTPUT_FILE"
        else
            echo "${prefix}[二进制文件，跳过内容]" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        fi
    else
        echo "${prefix}[二进制文件，跳过内容]" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
    fi
}

# 清空输出文件
> "$OUTPUT_FILE"

echo "输出文件: $OUTPUT_FILE"
echo ""

# 依次处理每个输入路径
first=1
for target in "$@"; do
    if [ ! -e "$target" ]; then
        echo "警告: 路径 '$target' 不存在，跳过" >&2
        continue
    fi

    # 转换为绝对路径
    abs_target=$(cd "$(dirname "$target")" && pwd)/$(basename "$target")

    # 多个输入源之间加分隔标题
    if [ $first -eq 0 ]; then
        echo "" >> "$OUTPUT_FILE"
    fi
    first=0

    echo "===== 来源: $abs_target =====" >> "$OUTPUT_FILE"

    if [ -d "$abs_target" ]; then
        echo "开始处理目录: $abs_target"
        process_directory "$abs_target" "" "true"
    else
        echo "开始处理文件: $abs_target"
        bname=$(basename "$abs_target")
        echo "$bname" >> "$OUTPUT_FILE"
        print_file_content "$abs_target" ""
    fi
    echo "" >> "$OUTPUT_FILE"
done

echo ""
echo "处理完成！结果保存在: $OUTPUT_FILE"
