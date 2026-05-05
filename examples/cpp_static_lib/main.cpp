/**
 * vault.a 静态库使用示例
 * 
 * 编译: g++ main.cpp vault.a -lsodium -o example
 * 运行: ./example
 */

#include <iostream>
#include <iomanip>
#include <cstring>

// 声明 vault.a 中导出的函数
extern "C" {
    int get_key(unsigned char* out, unsigned long long* out_len);
}

// 打印十六进制数据
void print_hex(const char* label, const unsigned char* data, size_t len) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)data[i];
    }
    std::cout << std::dec << "\n";
}

int main() {
    std::cout << "╔════════════════════════════════════════════╗\n";
    std::cout << "║  Vault Static Library Example             ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n\n";
    
    // 准备接收密钥的缓冲区
    unsigned char key[32];
    unsigned long long key_len = 0;
    
    std::cout << "Calling get_key()...\n";
    
    // 调用 vault.a 中的函数
    int result = get_key(key, &key_len);
    
    if (result == 0) {
        std::cout << "✓ Success!\n\n";
        std::cout << "Key length: " << key_len << " bytes\n";
        print_hex("Key (hex)", key, key_len);
        
        std::cout << "\nKey successfully decrypted from vault.a\n";
        std::cout << "You can now use this key for encryption/decryption.\n";
        
        // 使用完毕后立即清零
        std::memset(key, 0, sizeof(key));
        std::cout << "\n✓ Key cleared from memory\n";
        
        return 0;
    } else {
        std::cerr << "✗ Failed to get key!\n";
        std::cerr << "Error code: " << result << "\n\n";
        
        std::cerr << "Possible reasons:\n";
        std::cerr << "  1. vault.a not linked properly\n";
        std::cerr << "  2. libsodium not installed\n";
        std::cerr << "  3. Invalid parameters\n";
        
        return 1;
    }
}
