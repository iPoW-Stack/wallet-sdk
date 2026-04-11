/**
 * TCC Wrapper for Ubuntu x86_64
 * 
 * 提供与 Android 版本类似的接口，用于编译 C 代码为各种格式
 */

#ifndef TCC_WRAPPER_H
#define TCC_WRAPPER_H

#include <string>
#include <vector>
#include <cstdint>

namespace tcc {

enum class OutputType {
    OBJECT,      // .o 目标文件
    ARCHIVE,     // .a 静态库
    SHARED,      // .so 共享库
    EXECUTABLE,  // 可执行文件
    MEMORY       // 内存中执行
};

struct CompileResult {
    bool success;
    std::vector<uint8_t> binary;  // 编译后的二进制数据
    std::string error_message;
    size_t size;
    
    CompileResult() : success(false), size(0) {}
};

class TccCompiler {
public:
    TccCompiler();
    ~TccCompiler();
    
    /**
     * 编译 C 源码为指定格式
     * 
     * @param source_code  C 源代码
     * @param output_type  输出类型
     * @param options      额外的编译选项（如 "-O2 -Wall"）
     * @return             编译结果
     */
    CompileResult compile(
        const std::string& source_code,
        OutputType output_type = OutputType::OBJECT,
        const std::string& options = ""
    );
    
    /**
     * 编译 C 源码并保存到文件
     * 
     * @param source_code  C 源代码
     * @param output_file  输出文件路径
     * @param output_type  输出类型
     * @param options      额外的编译选项
     * @return             是否成功
     */
    bool compile_to_file(
        const std::string& source_code,
        const std::string& output_file,
        OutputType output_type = OutputType::OBJECT,
        const std::string& options = ""
    );
    
    /**
     * 编译多个源文件为静态库
     * 
     * @param source_files  源文件路径列表
     * @param output_file   输出 .a 文件路径
     * @return              是否成功
     */
    bool compile_archive(
        const std::vector<std::string>& source_files,
        const std::string& output_file
    );
    
    /**
     * 获取最后的错误信息
     */
    std::string get_last_error() const { return last_error_; }
    
private:
    std::string last_error_;
    std::string temp_dir_;
    
    void set_error(const std::string& msg);
    std::string get_temp_file(const std::string& suffix);
};

} // namespace tcc

#endif // TCC_WRAPPER_H
