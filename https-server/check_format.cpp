/**
 * 检查文件格式 - 在 Linux 服务器上编译运行
 * 
 * 编译: g++ -std=c++17 check_format.cpp -o check_format
 * 运行: ./check_format <file_path>
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <iomanip>

std::string detect_file_type(const std::string& data) {
    if (data.size() < 8) return "unknown (too small)";
    
    // 打印前 16 字节
    std::cout << "First 16 bytes (hex): ";
    for (size_t i = 0; i < 16 && i < data.size(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)(unsigned char)data[i] << " ";
    }
    std::cout << std::dec << "\n";
    
    std::cout << "First 16 bytes (ASCII): ";
    for (size_t i = 0; i < 16 && i < data.size(); i++) {
        char c = data[i];
        std::cout << (c >= 32 && c < 127 ? c : '.');
    }
    std::cout << "\n\n";
    
    // ELF 魔数: 0x7f 'E' 'L' 'F'
    if (data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        std::cout << "Format: ELF\n";
        
        if (data.size() >= 18) {
            uint16_t e_type = *reinterpret_cast<const uint16_t*>(&data[16]);
            std::cout << "e_type value: " << e_type << "\n";
            
            switch (e_type) {
                case 1:
                    std::cout << "Type: ET_REL (Relocatable / .o)\n";
                    return ".o";
                case 2:
                    std::cout << "Type: ET_EXEC (Executable)\n";
                    return ".exe";
                case 3:
                    std::cout << "Type: ET_DYN (Shared library / .so)\n";
                    return ".so";
                case 4:
                    std::cout << "Type: ET_CORE (Core file)\n";
                    return ".core";
                default:
                    std::cout << "Type: Unknown ELF type\n";
                    return ".elf";
            }
        }
        return ".so"; // 默认
    }
    
    // AR 魔数: "!<arch>\n"
    if (data.size() >= 8 && 
        data[0] == '!' && data[1] == '<' && 
        data[2] == 'a' && data[3] == 'r' &&
        data[4] == 'c' && data[5] == 'h' &&
        data[6] == '>' && data[7] == '\n') {
        std::cout << "Format: AR (Archive)\n";
        std::cout << "Type: Static library\n";
        return ".a";
    }
    
    std::cout << "Format: Unknown\n";
    return ".bin";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file_path>\n";
        std::cerr << "Example: " << argv[0] << " uploads/libs/mylib.so\n";
        return 1;
    }
    
    std::string filepath = argv[1];
    
    std::cout << "=== File Format Check ===\n";
    std::cout << "File: " << filepath << "\n\n";
    
    // 读取文件
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << filepath << "\n";
        return 1;
    }
    
    // 读取前 1KB 用于检测
    std::string data(1024, '\0');
    file.read(&data[0], 1024);
    size_t bytes_read = file.gcount();
    data.resize(bytes_read);
    
    std::cout << "File size: " << bytes_read << "+ bytes\n\n";
    
    // 检测格式
    std::string ext = detect_file_type(data);
    
    std::cout << "\nDetected extension: " << ext << "\n";
    std::cout << "=== Check Complete ===\n";
    
    return 0;
}
