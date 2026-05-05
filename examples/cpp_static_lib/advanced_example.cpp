/**
 * vault.a 高级使用示例
 * 
 * 展示:
 * 1. RAII 密钥管理
 * 2. 与 libsodium 集成
 * 3. 安全的内存处理
 * 4. 错误处理
 * 
 * 编译: g++ advanced_example.cpp vault.a -lsodium -o advanced
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <memory>
#include <sodium.h>

extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

// ═══════════════════════════════════════════════════════════════
//  安全密钥管理类 (RAII)
// ═══════════════════════════════════════════════════════════════

class SecureKey {
public:
    SecureKey() : valid_(false), len_(0) {
        sodium_memzero(data_, sizeof(data_));
        load();
    }
    
    ~SecureKey() {
        // 使用 libsodium 的安全清零
        sodium_memzero(data_, sizeof(data_));
    }
    
    // 禁止拷贝
    SecureKey(const SecureKey&) = delete;
    SecureKey& operator=(const SecureKey&) = delete;
    
    // 允许移动
    SecureKey(SecureKey&& other) noexcept {
        std::memcpy(data_, other.data_, sizeof(data_));
        len_ = other.len_;
        valid_ = other.valid_;
        sodium_memzero(other.data_, sizeof(other.data_));
        other.valid_ = false;
    }
    
    bool valid() const { return valid_; }
    const unsigned char* data() const { return valid_ ? data_ : nullptr; }
    size_t size() const { return valid_ ? len_ : 0; }
    
private:
    void load() {
        int result = get_key(data_, &len_);
        if (result == 0 && len_ == 32) {
            valid_ = true;
        } else {
            std::cerr << "Failed to load key (error: " << result << ")\n";
        }
    }
    
    unsigned char data_[32];
    unsigned long long len_;
    bool valid_;
};

// ═══════════════════════════════════════════════════════════════
//  加密/解密示例
// ═══════════════════════════════════════════════════════════════

class Encryptor {
public:
    Encryptor(const SecureKey& key) : key_(key) {}
    
    // 加密数据
    bool encrypt(const std::string& plaintext, std::vector<unsigned char>& ciphertext) {
        if (!key_.valid()) {
            std::cerr << "Invalid key\n";
            return false;
        }
        
        // 生成随机 nonce
        unsigned char nonce[crypto_secretbox_NONCEBYTES];
        randombytes_buf(nonce, sizeof(nonce));
        
        // 计算密文大小
        size_t ciphertext_len = crypto_secretbox_NONCEBYTES + 
                               crypto_secretbox_MACBYTES + 
                               plaintext.size();
        
        ciphertext.resize(ciphertext_len);
        
        // 存储 nonce
        std::memcpy(ciphertext.data(), nonce, crypto_secretbox_NONCEBYTES);
        
        // 加密
        if (crypto_secretbox_easy(
                ciphertext.data() + crypto_secretbox_NONCEBYTES,
                reinterpret_cast<const unsigned char*>(plaintext.data()),
                plaintext.size(),
                nonce,
                key_.data()) != 0) {
            std::cerr << "Encryption failed\n";
            return false;
        }
        
        // 清零 nonce
        sodium_memzero(nonce, sizeof(nonce));
        
        return true;
    }
    
    // 解密数据
    bool decrypt(const std::vector<unsigned char>& ciphertext, std::string& plaintext) {
        if (!key_.valid()) {
            std::cerr << "Invalid key\n";
            return false;
        }
        
        if (ciphertext.size() < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
            std::cerr << "Ciphertext too short\n";
            return false;
        }
        
        // 提取 nonce
        const unsigned char* nonce = ciphertext.data();
        
        // 计算明文大小
        size_t plaintext_len = ciphertext.size() - 
                              crypto_secretbox_NONCEBYTES - 
                              crypto_secretbox_MACBYTES;
        
        plaintext.resize(plaintext_len);
        
        // 解密
        if (crypto_secretbox_open_easy(
                reinterpret_cast<unsigned char*>(&plaintext[0]),
                ciphertext.data() + crypto_secretbox_NONCEBYTES,
                ciphertext.size() - crypto_secretbox_NONCEBYTES,
                nonce,
                key_.data()) != 0) {
            std::cerr << "Decryption failed (wrong key or corrupted data)\n";
            return false;
        }
        
        return true;
    }
    
private:
    const SecureKey& key_;
};

// ═══════════════════════════════════════════════════════════════
//  工具函数
// ═══════════════════════════════════════════════════════════════

void print_hex(const char* label, const unsigned char* data, size_t len) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)data[i];
    }
    std::cout << std::dec << "\n";
}

void print_hex(const char* label, const std::vector<unsigned char>& data) {
    print_hex(label, data.data(), data.size());
}

// ═══════════════════════════════════════════════════════════════
//  主程序
// ═══════════════════════════════════════════════════════════════

int main() {
    std::cout << "╔════════════════════════════════════════════╗\n";
    std::cout << "║  Advanced Vault Library Example           ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n\n";
    
    // 初始化 libsodium
    if (sodium_init() < 0) {
        std::cerr << "✗ Failed to initialize libsodium\n";
        return 1;
    }
    std::cout << "✓ libsodium initialized\n\n";
    
    // 1. 加载密钥 (RAII 自动管理)
    std::cout << "1. Loading key from vault.a...\n";
    SecureKey key;
    
    if (!key.valid()) {
        std::cerr << "✗ Failed to load key\n";
        return 1;
    }
    
    std::cout << "✓ Key loaded (" << key.size() << " bytes)\n";
    print_hex("   Key", key.data(), key.size());
    std::cout << "\n";
    
    // 2. 加密示例
    std::cout << "2. Encrypting data...\n";
    Encryptor enc(key);
    
    std::string message = "Hello, World! This is a secret message.";
    std::vector<unsigned char> ciphertext;
    
    if (!enc.encrypt(message, ciphertext)) {
        std::cerr << "✗ Encryption failed\n";
        return 1;
    }
    
    std::cout << "✓ Encrypted " << message.size() 
              << " bytes -> " << ciphertext.size() << " bytes\n";
    std::cout << "   Plaintext: \"" << message << "\"\n";
    print_hex("   Ciphertext", ciphertext);
    std::cout << "\n";
    
    // 3. 解密示例
    std::cout << "3. Decrypting data...\n";
    std::string decrypted;
    
    if (!enc.decrypt(ciphertext, decrypted)) {
        std::cerr << "✗ Decryption failed\n";
        return 1;
    }
    
    std::cout << "✓ Decrypted " << ciphertext.size() 
              << " bytes -> " << decrypted.size() << " bytes\n";
    std::cout << "   Decrypted: \"" << decrypted << "\"\n";
    std::cout << "\n";
    
    // 4. 验证
    std::cout << "4. Verification...\n";
    if (message == decrypted) {
        std::cout << "✓ Decryption successful! Messages match.\n";
    } else {
        std::cerr << "✗ Decryption failed! Messages don't match.\n";
        return 1;
    }
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════╗\n";
    std::cout << "║  All tests passed!                         ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n";
    
    // key 析构时自动清零
    return 0;
}
