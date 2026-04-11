/**
 * TCC Wrapper Implementation
 */

#include "tcc_wrapper.h"
#include "libtcc.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

namespace tcc {

// 错误回调
static void error_callback(void* opaque, const char* msg) {
    std::string* err = static_cast<std::string*>(opaque);
    if (err) {
        err->append(msg);
        err->append("\n");
    }
}

TccCompiler::TccCompiler() {
    // 使用 /tmp 作为临时目录
    temp_dir_ = "/tmp/tcc_" + std::to_string(getpid());
    mkdir(temp_dir_.c_str(), 0755);
}

TccCompiler::~TccCompiler() {
    // 清理临时目录
    std::string cmd = "rm -rf " + temp_dir_;
    system(cmd.c_str());
}

void TccCompiler::set_error(const std::string& msg) {
    last_error_ = msg;
}

std::string TccCompiler::get_temp_file(const std::string& suffix) {
    static int counter = 0;
    return temp_dir_ + "/temp_" + std::to_string(counter++) + suffix;
}

CompileResult TccCompiler::compile(
    const std::string& source_code,
    OutputType output_type,
    const std::string& options)
{
    CompileResult result;
    last_error_.clear();
    
    TCCState* tcc = tcc_new();
    if (!tcc) {
        set_error("Failed to create TCC state");
        result.error_message = last_error_;
        return result;
    }
    
    std::string error_buf;
    tcc_set_error_func(tcc, &error_buf, error_callback);
    
    // 设置选项
    if (!options.empty()) {
        tcc_set_options(tcc, options.c_str());
    }
    
    // 设置输出类型
    int tcc_output_type;
    switch (output_type) {
        case OutputType::OBJECT:
            tcc_output_type = TCC_OUTPUT_OBJ;
            break;
        case OutputType::SHARED:
            tcc_output_type = TCC_OUTPUT_DLL;
            tcc_set_options(tcc, "-shared -fPIC");
            break;
        case OutputType::EXECUTABLE:
            tcc_output_type = TCC_OUTPUT_EXE;
            break;
        case OutputType::MEMORY:
            tcc_output_type = TCC_OUTPUT_MEMORY;
            break;
        case OutputType::ARCHIVE:
            // Archive 需要先编译为 .o，然后用 ar 打包
            tcc_output_type = TCC_OUTPUT_OBJ;
            break;
        default:
            tcc_output_type = TCC_OUTPUT_OBJ;
    }
    
    tcc_set_output_type(tcc, tcc_output_type);
    
    // 编译源码
    if (tcc_compile_string(tcc, source_code.c_str()) != 0) {
        set_error("Compilation failed: " + error_buf);
        result.error_message = last_error_;
        tcc_delete(tcc);
        return result;
    }
    
    // 输出到临时文件
    std::string temp_file = get_temp_file(
        output_type == OutputType::SHARED ? ".so" : ".o"
    );
    
    if (tcc_output_file(tcc, temp_file.c_str()) != 0) {
        set_error("Failed to write output file: " + error_buf);
        result.error_message = last_error_;
        tcc_delete(tcc);
        return result;
    }
    
    tcc_delete(tcc);
    
    // 读取文件到内存
    std::ifstream file(temp_file, std::ios::binary | std::ios::ate);
    if (!file) {
        set_error("Failed to read output file");
        result.error_message = last_error_;
        return result;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    result.binary.resize(size);
    if (!file.read(reinterpret_cast<char*>(result.binary.data()), size)) {
        set_error("Failed to read file content");
        result.error_message = last_error_;
        return result;
    }
    
    result.success = true;
    result.size = size;
    
    // 清理临时文件
    unlink(temp_file.c_str());
    
    return result;
}

bool TccCompiler::compile_to_file(
    const std::string& source_code,
    const std::string& output_file,
    OutputType output_type,
    const std::string& options)
{
    last_error_.clear();
    
    TCCState* tcc = tcc_new();
    if (!tcc) {
        set_error("Failed to create TCC state");
        return false;
    }
    
    std::string error_buf;
    tcc_set_error_func(tcc, &error_buf, error_callback);
    
    // 设置选项
    if (!options.empty()) {
        tcc_set_options(tcc, options.c_str());
    }
    
    // 设置输出类型
    int tcc_output_type;
    switch (output_type) {
        case OutputType::OBJECT:
            tcc_output_type = TCC_OUTPUT_OBJ;
            break;
        case OutputType::SHARED:
            tcc_output_type = TCC_OUTPUT_DLL;
            tcc_set_options(tcc, "-shared -fPIC");
            break;
        case OutputType::EXECUTABLE:
            tcc_output_type = TCC_OUTPUT_EXE;
            break;
        case OutputType::ARCHIVE:
            tcc_output_type = TCC_OUTPUT_OBJ;
            break;
        default:
            tcc_output_type = TCC_OUTPUT_OBJ;
    }
    
    tcc_set_output_type(tcc, tcc_output_type);
    
    // 编译源码
    if (tcc_compile_string(tcc, source_code.c_str()) != 0) {
        set_error("Compilation failed: " + error_buf);
        tcc_delete(tcc);
        return false;
    }
    
    // 对于 ARCHIVE 类型，先输出 .o 文件
    std::string actual_output = output_file;
    if (output_type == OutputType::ARCHIVE) {
        actual_output = get_temp_file(".o");
    }
    
    if (tcc_output_file(tcc, actual_output.c_str()) != 0) {
        set_error("Failed to write output file: " + error_buf);
        tcc_delete(tcc);
        return false;
    }
    
    tcc_delete(tcc);
    
    // 如果是 ARCHIVE，使用 ar 创建静态库
    if (output_type == OutputType::ARCHIVE) {
        std::string cmd = "ar rcs " + output_file + " " + actual_output;
        int ret = system(cmd.c_str());
        unlink(actual_output.c_str());
        
        if (ret != 0) {
            set_error("Failed to create archive with ar");
            return false;
        }
    }
    
    return true;
}

bool TccCompiler::compile_archive(
    const std::vector<std::string>& source_files,
    const std::string& output_file)
{
    last_error_.clear();
    
    if (source_files.empty()) {
        set_error("No source files provided");
        return false;
    }
    
    std::vector<std::string> object_files;
    
    // 编译每个源文件为 .o
    for (const auto& src_file : source_files) {
        // 读取源文件
        std::ifstream file(src_file);
        if (!file) {
            set_error("Failed to read source file: " + src_file);
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source_code = buffer.str();
        
        // 编译为 .o
        std::string obj_file = get_temp_file(".o");
        if (!compile_to_file(source_code, obj_file, OutputType::OBJECT)) {
            return false;
        }
        
        object_files.push_back(obj_file);
    }
    
    // 使用 ar 创建静态库
    std::string cmd = "ar rcs " + output_file;
    for (const auto& obj : object_files) {
        cmd += " " + obj;
    }
    
    int ret = system(cmd.c_str());
    
    // 清理临时 .o 文件
    for (const auto& obj : object_files) {
        unlink(obj.c_str());
    }
    
    if (ret != 0) {
        set_error("Failed to create archive with ar");
        return false;
    }
    
    return true;
}

} // namespace tcc
