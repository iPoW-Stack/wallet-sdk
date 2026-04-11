/**
 * TCC Wrapper 使用示例
 * 
 * 演示如何使用 TCC 动态编译 C 代码并生成静态库
 */

#include "tcc_wrapper.h"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <source.c> [output.a]\n";
        std::cout << "\nExample:\n";
        std::cout << "  " << argv[0] << " mycode.c libmycode.a\n";
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_file = argc > 2 ? argv[2] : "output.a";
    
    // 读取源文件
    std::ifstream file(input_file);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << input_file << "\n";
        return 1;
    }
    
    std::string source_code(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    
    std::cout << "Compiling " << input_file << " ...\n";
    
    // 创建编译器
    tcc::TccCompiler compiler;
    
    // 编译为静态库
    bool success = compiler.compile_to_file(
        source_code,
        output_file,
        tcc::OutputType::ARCHIVE,
        "-O2 -Wall"
    );
    
    if (!success) {
        std::cerr << "Compilation failed:\n";
        std::cerr << compiler.get_last_error() << "\n";
        return 1;
    }
    
    std::cout << "✓ Successfully compiled to: " << output_file << "\n";
    
    // 显示文件大小
    std::ifstream out_file(output_file, std::ios::binary | std::ios::ate);
    if (out_file) {
        size_t size = out_file.tellg();
        std::cout << "  File size: " << size << " bytes\n";
    }
    
    // 显示内容
    std::cout << "\nArchive contents:\n";
    std::string cmd = "ar -t " + output_file;
    system(cmd.c_str());
    
    return 0;
}
