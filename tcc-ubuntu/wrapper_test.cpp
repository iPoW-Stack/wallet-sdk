/**
 * TCC Wrapper Test
 * 
 * 测试 C++ 封装库的功能
 */

#include "tcc_wrapper.h"
#include <iostream>
#include <fstream>
#include <cstdio>

using namespace tcc;

void print_header(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

bool test_compile_object() {
    print_header("Test 1: Compile to Object File");
    
    std::string source = R"(
        int add(int a, int b) {
            return a + b;
        }
        int subtract(int a, int b) {
            return a - b;
        }
    )";
    
    TccCompiler compiler;
    auto result = compiler.compile(source, OutputType::OBJECT);
    
    if (!result.success) {
        std::cerr << "✗ Failed: " << result.error_message << "\n";
        return false;
    }
    
    std::cout << "✓ Compiled successfully\n";
    std::cout << "  Binary size: " << result.size << " bytes\n";
    std::cout << "  First 16 bytes (hex): ";
    for (size_t i = 0; i < 16 && i < result.binary.size(); i++) {
        printf("%02x ", result.binary[i]);
    }
    std::cout << "\n";
    
    return true;
}

bool test_compile_to_file() {
    print_header("Test 2: Compile to File");
    
    std::string source = R"(
        int multiply(int a, int b) {
            return a * b;
        }
    )";
    
    TccCompiler compiler;
    bool success = compiler.compile_to_file(
        source,
        "test_output.o",
        OutputType::OBJECT
    );
    
    if (!success) {
        std::cerr << "✗ Failed: " << compiler.get_last_error() << "\n";
        return false;
    }
    
    // 检查文件是否存在
    std::ifstream file("test_output.o", std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "✗ Output file not found\n";
        return false;
    }
    
    size_t size = file.tellg();
    std::cout << "✓ File created: test_output.o (" << size << " bytes)\n";
    
    remove("test_output.o");
    return true;
}

bool test_compile_archive() {
    print_header("Test 3: Compile to Static Library (.a)");
    
    std::string source = R"(
        int power(int base, int exp) {
            int result = 1;
            for (int i = 0; i < exp; i++) {
                result *= base;
            }
            return result;
        }
    )";
    
    TccCompiler compiler;
    bool success = compiler.compile_to_file(
        source,
        "libtest.a",
        OutputType::ARCHIVE
    );
    
    if (!success) {
        std::cerr << "✗ Failed: " << compiler.get_last_error() << "\n";
        return false;
    }
    
    // 检查文件
    std::ifstream file("libtest.a", std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "✗ Archive file not found\n";
        return false;
    }
    
    size_t size = file.tellg();
    std::cout << "✓ Archive created: libtest.a (" << size << " bytes)\n";
    std::cout << "  View contents: ar -t libtest.a\n";
    
    // 查看内容
    system("ar -t libtest.a");
    
    remove("libtest.a");
    return true;
}

bool test_compile_shared() {
    print_header("Test 4: Compile to Shared Library (.so)");
    
    std::string source = R"(
        int factorial(int n) {
            if (n <= 1) return 1;
            return n * factorial(n - 1);
        }
    )";
    
    TccCompiler compiler;
    bool success = compiler.compile_to_file(
        source,
        "libtest.so",
        OutputType::SHARED
    );
    
    if (!success) {
        std::cerr << "✗ Failed: " << compiler.get_last_error() << "\n";
        return false;
    }
    
    std::ifstream file("libtest.so", std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "✗ Shared library not found\n";
        return false;
    }
    
    size_t size = file.tellg();
    std::cout << "✓ Shared library created: libtest.so (" << size << " bytes)\n";
    std::cout << "  Check symbols: nm -D libtest.so | grep factorial\n";
    
    system("nm -D libtest.so 2>/dev/null | grep factorial || echo '  (nm not available)'");
    
    remove("libtest.so");
    return true;
}

bool test_compile_with_options() {
    print_header("Test 5: Compile with Options");
    
    std::string source = R"(
        #include <string.h>
        int string_length(const char* s) {
            return strlen(s);
        }
    )";
    
    TccCompiler compiler;
    auto result = compiler.compile(
        source,
        OutputType::OBJECT,
        "-O2 -Wall"
    );
    
    if (!result.success) {
        std::cerr << "✗ Failed: " << result.error_message << "\n";
        return false;
    }
    
    std::cout << "✓ Compiled with options: -O2 -Wall\n";
    std::cout << "  Binary size: " << result.size << " bytes\n";
    
    return true;
}

bool test_compile_multiple_files() {
    print_header("Test 6: Compile Multiple Files to Archive");
    
    // 创建临时源文件
    std::ofstream file1("temp_src1.c");
    file1 << "int func1() { return 1; }\n";
    file1.close();
    
    std::ofstream file2("temp_src2.c");
    file2 << "int func2() { return 2; }\n";
    file2.close();
    
    TccCompiler compiler;
    std::vector<std::string> sources = {"temp_src1.c", "temp_src2.c"};
    bool success = compiler.compile_archive(sources, "libmulti.a");
    
    remove("temp_src1.c");
    remove("temp_src2.c");
    
    if (!success) {
        std::cerr << "✗ Failed: " << compiler.get_last_error() << "\n";
        return false;
    }
    
    std::cout << "✓ Multi-file archive created: libmulti.a\n";
    std::cout << "  Contents:\n";
    system("ar -t libmulti.a");
    
    remove("libmulti.a");
    return true;
}

int main() {
    std::cout << "╔════════════════════════════════════════════╗\n";
    std::cout << "║  TCC C++ Wrapper Test - Ubuntu x86_64      ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n";
    
    int failed = 0;
    
    if (!test_compile_object()) failed++;
    if (!test_compile_to_file()) failed++;
    if (!test_compile_archive()) failed++;
    if (!test_compile_shared()) failed++;
    if (!test_compile_with_options()) failed++;
    if (!test_compile_multiple_files()) failed++;
    
    std::cout << "\n═══════════════════════════════════════════\n";
    if (failed == 0) {
        std::cout << "✓ All tests passed!\n";
    } else {
        std::cout << "✗ " << failed << " test(s) failed\n";
    }
    std::cout << "═══════════════════════════════════════════\n";
    
    return failed;
}
