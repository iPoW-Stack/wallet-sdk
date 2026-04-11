/**
 * 测试文件格式检测逻辑
 */

#include <iostream>
#include <string>
#include <cstdint>

static std::string detect_file_type(const std::string& data) {
    if (data.size() < 8) return ".bin";
    
    // 调试：打印前 16 字节
    std::cout << "[DEBUG] File header (first 16 bytes): ";
    for (size_t i = 0; i < 16 && i < data.size(); i++) {
        printf("%02x ", (unsigned char)data[i]);
    }
    std::cout << "\n";
    std::cout << "[DEBUG] As ASCII: ";
    for (size_t i = 0; i < 16 && i < data.size(); i++) {
        char c = data[i];
        std::cout << (c >= 32 && c < 127 ? c : '.');
    }
    std::cout << "\n";
    
    // ELF 魔数: 0x7f 'E' 'L' 'F'
    if (data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        // 检查 ELF 类型
        if (data.size() >= 18) {
            uint16_t e_type = *reinterpret_cast<const uint16_t*>(&data[16]);
            std::cout << "[DEBUG] ELF e_type = " << e_type << "\n";
            // ET_DYN (3) = 共享库
            if (e_type == 3) return ".so";
            // ET_REL (1) = 可重定位文件 (.o)
            if (e_type == 1) return ".o";
        }
        return ".so"; // 默认为共享库
    }
    
    // AR 魔数: "!<arch>\n"
    std::cout << "[DEBUG] Checking AR magic: ";
    std::cout << "data[0]=" << (int)(unsigned char)data[0] << " ";
    std::cout << "data[1]=" << (int)(unsigned char)data[1] << " ";
    std::cout << "data[2]=" << (int)(unsigned char)data[2] << " ";
    std::cout << "data[3]=" << (int)(unsigned char)data[3] << " ";
    std::cout << "data[4]=" << (int)(unsigned char)data[4] << " ";
    std::cout << "data[5]=" << (int)(unsigned char)data[5] << " ";
    std::cout << "data[6]=" << (int)(unsigned char)data[6] << " ";
    std::cout << "data[7]=" << (int)(unsigned char)data[7] << "\n";
    
    if (data.size() >= 8 && 
        data[0] == '!' && data[1] == '<' && 
        data[2] == 'a' && data[3] == 'r' &&
        data[4] == 'c' && data[5] == 'h' &&
        data[6] == '>' && data[7] == '\n') {
        std::cout << "[DEBUG] Detected AR format\n";
        return ".a";
    }
    
    std::cout << "[DEBUG] Unknown format\n";
    return ".bin"; // 未知格式
}

int main() {
    std::cout << "=== File Type Detection Test ===\n\n";
    
    // 测试 1: AR 格式
    std::cout << "Test 1: AR format\n";
    std::string ar_data = "!<arch>\n";
    ar_data += std::string(60, ' '); // 文件头
    std::string result1 = detect_file_type(ar_data);
    std::cout << "Result: " << result1 << "\n";
    std::cout << "Expected: .a\n";
    std::cout << (result1 == ".a" ? "✓ PASS" : "✗ FAIL") << "\n\n";
    
    // 测试 2: ELF 共享库
    std::cout << "Test 2: ELF shared library (.so)\n";
    std::string elf_so;
    elf_so.push_back(0x7f);
    elf_so.push_back('E');
    elf_so.push_back('L');
    elf_so.push_back('F');
    elf_so += std::string(12, '\0');
    elf_so.push_back(3); // e_type = ET_DYN
    elf_so.push_back(0);
    std::string result2 = detect_file_type(elf_so);
    std::cout << "Result: " << result2 << "\n";
    std::cout << "Expected: .so\n";
    std::cout << (result2 == ".so" ? "✓ PASS" : "✗ FAIL") << "\n\n";
    
    // 测试 3: ELF 目标文件
    std::cout << "Test 3: ELF object file (.o)\n";
    std::string elf_o;
    elf_o.push_back(0x7f);
    elf_o.push_back('E');
    elf_o.push_back('L');
    elf_o.push_back('F');
    elf_o += std::string(12, '\0');
    elf_o.push_back(1); // e_type = ET_REL
    elf_o.push_back(0);
    std::string result3 = detect_file_type(elf_o);
    std::cout << "Result: " << result3 << "\n";
    std::cout << "Expected: .o\n";
    std::cout << (result3 == ".o" ? "✓ PASS" : "✗ FAIL") << "\n\n";
    
    return 0;
}
