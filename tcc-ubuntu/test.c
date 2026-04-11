/**
 * TCC Compiler Test for Ubuntu x86_64
 * 
 * 测试 libtcc 编译 C 代码并生成静态库 (.a)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libtcc.h"

// 错误回调
static void tcc_error_callback(void* opaque, const char* msg) {
    fprintf(stderr, "TCC Error: %s\n", msg);
}

// 测试 1: 编译为目标文件 (.o)
int test_compile_obj() {
    printf("=== Test 1: Compile to Object File (.o) ===\n");
    
    const char* source = 
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n"
        "int multiply(int a, int b) {\n"
        "    return a * b;\n"
        "}\n";
    
    TCCState* tcc = tcc_new();
    if (!tcc) {
        fprintf(stderr, "Failed to create TCC state\n");
        return -1;
    }
    
    tcc_set_error_func(tcc, NULL, tcc_error_callback);
    tcc_set_output_type(tcc, TCC_OUTPUT_OBJ);
    
    if (tcc_compile_string(tcc, source) != 0) {
        fprintf(stderr, "Compilation failed\n");
        tcc_delete(tcc);
        return -1;
    }
    
    if (tcc_output_file(tcc, "test_output.o") != 0) {
        fprintf(stderr, "Failed to write object file\n");
        tcc_delete(tcc);
        return -1;
    }
    
    tcc_delete(tcc);
    printf("✓ Generated: test_output.o\n\n");
    return 0;
}

// 测试 2: 编译为静态库 (.a)
int test_compile_archive() {
    printf("=== Test 2: Compile to Static Library (.a) ===\n");
    
    const char* source = 
        "#include <string.h>\n"
        "int string_length(const char* s) {\n"
        "    return strlen(s);\n"
        "}\n"
        "void reverse_string(char* s) {\n"
        "    int len = strlen(s);\n"
        "    for (int i = 0; i < len/2; i++) {\n"
        "        char tmp = s[i];\n"
        "        s[i] = s[len-1-i];\n"
        "        s[len-1-i] = tmp;\n"
        "    }\n"
        "}\n";
    
    // 步骤 1: 编译为 .o
    TCCState* tcc = tcc_new();
    if (!tcc) {
        fprintf(stderr, "Failed to create TCC state\n");
        return -1;
    }
    
    tcc_set_error_func(tcc, NULL, tcc_error_callback);
    tcc_set_output_type(tcc, TCC_OUTPUT_OBJ);
    
    if (tcc_compile_string(tcc, source) != 0) {
        fprintf(stderr, "Compilation failed\n");
        tcc_delete(tcc);
        return -1;
    }
    
    if (tcc_output_file(tcc, "libtest.o") != 0) {
        fprintf(stderr, "Failed to write object file\n");
        tcc_delete(tcc);
        return -1;
    }
    
    tcc_delete(tcc);
    
    // 步骤 2: 使用 ar 创建静态库
    int ret = system("ar rcs libtest.a libtest.o");
    if (ret != 0) {
        fprintf(stderr, "Failed to create archive\n");
        return -1;
    }
    
    printf("✓ Generated: libtest.a\n");
    printf("  View contents: ar -t libtest.a\n\n");
    return 0;
}

// 测试 3: 编译为共享库 (.so)
int test_compile_shared() {
    printf("=== Test 3: Compile to Shared Library (.so) ===\n");
    
    const char* source = 
        "int factorial(int n) {\n"
        "    if (n <= 1) return 1;\n"
        "    return n * factorial(n - 1);\n"
        "}\n";
    
    TCCState* tcc = tcc_new();
    if (!tcc) {
        fprintf(stderr, "Failed to create TCC state\n");
        return -1;
    }
    
    tcc_set_error_func(tcc, NULL, tcc_error_callback);
    tcc_set_options(tcc, "-shared -fPIC");
    tcc_set_output_type(tcc, TCC_OUTPUT_DLL);
    
    if (tcc_compile_string(tcc, source) != 0) {
        fprintf(stderr, "Compilation failed\n");
        tcc_delete(tcc);
        return -1;
    }
    
    if (tcc_output_file(tcc, "libtest.so") != 0) {
        fprintf(stderr, "Failed to write shared library\n");
        tcc_delete(tcc);
        return -1;
    }
    
    tcc_delete(tcc);
    printf("✓ Generated: libtest.so\n");
    printf("  Check symbols: nm -D libtest.so\n\n");
    return 0;
}

// 测试 4: 编译并在内存中执行
int test_compile_memory() {
    printf("=== Test 4: Compile and Execute in Memory ===\n");
    
    const char* source = 
        "#include <stdio.h>\n"
        "int main() {\n"
        "    printf(\"Hello from TCC!\\n\");\n"
        "    return 42;\n"
        "}\n";
    
    TCCState* tcc = tcc_new();
    if (!tcc) {
        fprintf(stderr, "Failed to create TCC state\n");
        return -1;
    }
    
    tcc_set_error_func(tcc, NULL, tcc_error_callback);
    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
    
    if (tcc_compile_string(tcc, source) != 0) {
        fprintf(stderr, "Compilation failed\n");
        tcc_delete(tcc);
        return -1;
    }
    
    if (tcc_relocate(tcc, TCC_RELOCATE_AUTO) < 0) {
        fprintf(stderr, "Relocation failed\n");
        tcc_delete(tcc);
        return -1;
    }
    
    // 获取 main 函数指针
    int (*main_func)() = tcc_get_symbol(tcc, "main");
    if (!main_func) {
        fprintf(stderr, "Failed to get main symbol\n");
        tcc_delete(tcc);
        return -1;
    }
    
    printf("Executing compiled code:\n");
    int result = main_func();
    printf("Return value: %d\n\n", result);
    
    tcc_delete(tcc);
    return 0;
}

int main() {
    printf("╔════════════════════════════════════════════╗\n");
    printf("║  TCC Compiler Test - Ubuntu x86_64        ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    int failed = 0;
    
    if (test_compile_obj() != 0) failed++;
    if (test_compile_archive() != 0) failed++;
    if (test_compile_shared() != 0) failed++;
    if (test_compile_memory() != 0) failed++;
    
    printf("═══════════════════════════════════════════\n");
    if (failed == 0) {
        printf("✓ All tests passed!\n");
    } else {
        printf("✗ %d test(s) failed\n", failed);
    }
    printf("═══════════════════════════════════════════\n");
    
    // 清理测试文件
    system("rm -f test_output.o libtest.o libtest.a libtest.so");
    
    return failed;
}
