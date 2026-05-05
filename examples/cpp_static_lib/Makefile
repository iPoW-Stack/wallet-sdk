CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lsodium

# 静态库文件（需要从 VaultUploader 获取）
VAULT_LIB = vault.a

TARGET = example
ADVANCED = advanced

.PHONY: all clean run run-advanced help

all: $(TARGET) $(ADVANCED)

$(TARGET): main.cpp $(VAULT_LIB)
	@echo "Compiling basic example..."
	$(CXX) $(CXXFLAGS) main.cpp $(VAULT_LIB) $(LDFLAGS) -o $@
	@echo "✓ Build complete: $(TARGET)"

$(ADVANCED): advanced_example.cpp $(VAULT_LIB)
	@echo "Compiling advanced example..."
	$(CXX) $(CXXFLAGS) advanced_example.cpp $(VAULT_LIB) $(LDFLAGS) -o $@
	@echo "✓ Build complete: $(ADVANCED)"

run: $(TARGET)
	@echo "Running basic example..."
	@echo ""
	./$(TARGET)

run-advanced: $(ADVANCED)
	@echo "Running advanced example..."
	@echo ""
	./$(ADVANCED)

clean:
	rm -f $(TARGET) $(ADVANCED)
	@echo "✓ Cleaned"

help:
	@echo "Usage:"
	@echo "  make              - Build all examples"
	@echo "  make run          - Build and run basic example"
	@echo "  make run-advanced - Build and run advanced example"
	@echo "  make clean        - Remove build artifacts"
	@echo ""
	@echo "Requirements:"
	@echo "  1. Place vault.a in this directory"
	@echo "  2. Install libsodium: sudo apt-get install libsodium-dev"
	@echo ""
	@echo "Get vault.a:"
	@echo "  - Use VaultUploader app to generate"
	@echo "  - Or download from server"
	@echo "  - Or pull from Android: adb pull /data/data/.../vault.a"

$(VAULT_LIB):
	@echo "Error: $(VAULT_LIB) not found!"
	@echo ""
	@echo "Please obtain vault.a using one of these methods:"
	@echo "  1. Use VaultUploader Android app"
	@echo "  2. Download from HTTPS server"
	@echo "  3. Pull from device: adb pull /data/data/com.example.vaultuploader/files/vault.a"
	@echo ""
	@exit 1
