#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
多语言固件自动打包脚本
自动为每个语言版本编译固件并重命名保存
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# 项目根目录
PROJECT_ROOT = Path(__file__).parent.parent
SDKCONFIG_PATH = PROJECT_ROOT / "sdkconfig"
CMAKELISTS_PATH = PROJECT_ROOT / "CMakeLists.txt"
BUILD_DIR = PROJECT_ROOT / "build"
OUTPUT_BIN = BUILD_DIR / "xiaozhi.bin"

# 目标存档目录
TARGET_DIR = Path(r"D:\Desktop\固件存档\多语言固件存档")

# 语言配置映射
LANGUAGES = {
    'zh-CN': 'CONFIG_LANGUAGE_ZH_CN',
    'zh-TW': 'CONFIG_LANGUAGE_ZH_TW',
    'en-US': 'CONFIG_LANGUAGE_EN_US',
    'ja-JP': 'CONFIG_LANGUAGE_JA_JP',
    'ko-KR': 'CONFIG_LANGUAGE_KO_KR',
    'th-TH': 'CONFIG_LANGUAGE_TH_TH',
    'vi-VN': 'CONFIG_LANGUAGE_VI_VN'
}


def get_project_version():
    """从 CMakeLists.txt 读取项目版本号"""
    try:
        with open(CMAKELISTS_PATH, 'r', encoding='utf-8') as f:
            content = f.read()
            match = re.search(r'set\(PROJECT_VER\s+"([^"]+)"\)', content)
            if match:
                return match.group(1)
            else:
                print("❌ 无法从 CMakeLists.txt 中找到版本号")
                sys.exit(1)
    except Exception as e:
        print(f"❌ 读取 CMakeLists.txt 失败: {e}")
        sys.exit(1)


def modify_language_config(target_lang_config):
    """修改 sdkconfig 文件，只启用指定语言"""
    try:
        with open(SDKCONFIG_PATH, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        modified_lines = []
        for line in lines:
            # 检查是否是语言配置行
            is_lang_line = False
            for lang_code, config_name in LANGUAGES.items():
                if config_name in line:
                    is_lang_line = True
                    if config_name == target_lang_config:
                        # 启用目标语言
                        modified_lines.append(f"{config_name}=y\n")
                    else:
                        # 禁用其他语言
                        modified_lines.append(f"# {config_name} is not set\n")
                    break
            
            if not is_lang_line:
                modified_lines.append(line)
        
        # 写回文件
        with open(SDKCONFIG_PATH, 'w', encoding='utf-8') as f:
            f.writelines(modified_lines)
        
        return True
    except Exception as e:
        print(f"❌ 修改 sdkconfig 失败: {e}")
        return False


def build_firmware():
    """执行固件编译"""
    try:
        print("  ⏳ 开始编译...")
        # Windows 上使用 shell=True 以便找到 idf.py 命令
        result = subprocess.run(
            "idf.py build",
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            shell=True
        )
        
        if result.returncode == 0:
            print("  ✅ 编译成功")
            return True
        else:
            print("  ❌ 编译失败")
            print(result.stderr)
            return False
    except Exception as e:
        print(f"  ❌ 编译过程出错: {e}")
        return False


def copy_and_rename_firmware(version, lang_code):
    """复制并重命名固件文件"""
    try:
        # 检查源文件是否存在
        if not OUTPUT_BIN.exists():
            print(f"  ❌ 找不到编译输出文件: {OUTPUT_BIN}")
            return False
        
        # 确保目标目录存在
        TARGET_DIR.mkdir(parents=True, exist_ok=True)
        
        # 生成目标文件名
        target_filename = f"xq{version}ota-{lang_code}.bin"
        target_path = TARGET_DIR / target_filename
        
        # 复制文件
        shutil.copy2(OUTPUT_BIN, target_path)
        print(f"  ✅ 已保存: {target_filename}")
        return True
    except Exception as e:
        print(f"  ❌ 复制文件失败: {e}")
        return False


def check_idf_environment():
    """检查 ESP-IDF 环境是否已激活"""
    idf_path = os.environ.get('IDF_PATH')
    if not idf_path:
        print("❌ 错误: 未检测到 ESP-IDF 环境！")
        print("\n🔧 推荐方法 - 直接运行自动化脚本:")
        print(f"   PowerShell: .\\scripts\\build_multilang.ps1")
        print("\n📝 或者手动激活环境后运行:")
        print("   1. 激活 ESP-IDF 环境:")
        print("      & 'c:\\Users\\1\\.windsurf\\extensions\\espressif.esp-idf-extension-1.10.2-universal\\export.ps1'")
        print("   2. 运行 Python 脚本:")
        print("      python scripts\\build_multilang.py")
        return False
    
    # 测试 idf.py 是否可用
    try:
        result = subprocess.run(
            "idf.py --version",
            capture_output=True,
            text=True,
            shell=True,
            timeout=5
        )
        if result.returncode != 0:
            print("❌ 错误: idf.py 命令不可用")
            return False
    except Exception as e:
        print(f"❌ 错误: 无法执行 idf.py 命令: {e}")
        return False
    
    return True


def main():
    """主流程"""
    print("=" * 60)
    print("🚀 多语言固件自动打包工具")
    print("=" * 60)
    
    # 检查环境
    if not check_idf_environment():
        sys.exit(1)
    
    # 获取版本号
    version = get_project_version()
    print(f"📦 项目版本: {version}")
    print(f"📁 输出目录: {TARGET_DIR}")
    print(f"🌍 语言列表: {', '.join(LANGUAGES.keys())}")
    print("=" * 60)
    
    # 记录结果
    success_count = 0
    failed_langs = []
    
    # 遍历所有语言
    for lang_code, config_name in LANGUAGES.items():
        print(f"\n📌 处理语言: {lang_code} ({config_name})")
        
        # 1. 修改配置
        if not modify_language_config(config_name):
            failed_langs.append(lang_code)
            continue
        
        # 2. 编译固件
        if not build_firmware():
            failed_langs.append(lang_code)
            continue
        
        # 3. 复制重命名
        if not copy_and_rename_firmware(version, lang_code):
            failed_langs.append(lang_code)
            continue
        
        success_count += 1
    
    # 输出汇总报告
    print("\n" + "=" * 60)
    print("📊 打包完成汇总")
    print("=" * 60)
    print(f"✅ 成功: {success_count}/{len(LANGUAGES)}")
    if failed_langs:
        print(f"❌ 失败: {', '.join(failed_langs)}")
    else:
        print("🎉 全部语言打包成功！")
    print(f"📁 文件位置: {TARGET_DIR}")
    print("=" * 60)


if __name__ == "__main__":
    main()
