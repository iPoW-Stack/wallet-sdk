# TCC Compiler for Ubuntu x86_64
# 编译生成静态库 (.a) 和共享库 (.so)

CC       := gcc
CXX      := g++
AR       := ar
CFLAGS   := -O2 -fPIC -Wall -Wno-unused-function -Wno-unused-variable
CXXFLAGS := -std=c++17 -O2 -fPIC -Wall

# TCC 源码路径（复用 Android 项目中的 libtcc）
TCC_SRC_DIR := ../TccCompiler/tcclib/src/main/cpp/libtcc

# TCC 编译选项
TCC_DEFS := \
    -DONE_SOURCE=1 \
    -DCONFIG_TCC_STATIC=1 \
    -DCONFIG_TCC_SEMLOCK=0 \
    -DCONFIG_TCC_BACKTRACE=0 \
    -DCONFIG_TCC_BCHECK=0 \
    -DCONFIG_NEW_MACHO=0 \
    -DTCC_TARGET_X86_64 \
    -DCONFIG_SYSROOT=\"\" \
    -DCONFIG_TCCDIR=\"\" \
    -DCONFIG_TCC_SYSINCLUDEPATHS=\"\" \
    -DCONFIG_TCC_LIBPATHS=\"\" \
    -DCONFIG_TCC_CRTPREFIX=\"\" \
    -DCONFIG_TCC_ELFINTERP=\"\"

# 输出文件
LIBTCC_A  := libtcc_x86_64.a
LIBTCC_SO := libtcc_x86_64.so
WRAPPER_A := libtcc_wrapper.a
WRAPPER_SO := libtcc_wrapper.so
TEST_BIN  := tcc_test
WRAPPER_TEST := wrapper_test
EXAMPLE := tcc_example

.PHONY: all clean test wrapper-test example

all: $(LIBTCC_A) $(LIBTCC_SO) $(WRAPPER_A) $(WRAPPER_SO)

# 编译 libtcc.c 为目标文件
libtcc.o: $(TCC_SRC_DIR)/libtcc.c
	$(CC) $(CFLAGS) $(TCC_DEFS) -I$(TCC_SRC_DIR) -c $< -o $@

# 生成静态库
$(LIBTCC_A): libtcc.o
	$(AR) rcs $@ $^
	@echo "✓ Static library: $@"

# 生成共享库
$(LIBTCC_SO): libtcc.o
	$(CC) -shared -o $@ $^ -ldl -lpthread
	@echo "✓ Shared library: $@"

# C++ 封装库
tcc_wrapper.o: tcc_wrapper.cpp tcc_wrapper.h
	$(CXX) $(CXXFLAGS) -I$(TCC_SRC_DIR) -c tcc_wrapper.cpp -o $@

$(WRAPPER_A): tcc_wrapper.o $(LIBTCC_A)
	$(AR) rcs $@ tcc_wrapper.o
	@echo "✓ Wrapper static library: $@"

$(WRAPPER_SO): tcc_wrapper.o $(LIBTCC_SO)
	$(CXX) -shared -o $@ tcc_wrapper.o -L. -ltcc_x86_64 -ldl -lpthread
	@echo "✓ Wrapper shared library: $@"

# 测试程序
test: $(LIBTCC_A) test.c
	$(CC) $(CFLAGS) -I$(TCC_SRC_DIR) test.c $(LIBTCC_A) -o $(TEST_BIN) -ldl -lpthread
	@echo "✓ Test binary: $(TEST_BIN)"
	@echo ""
	@echo "Run test:"
	@echo "  ./$(TEST_BIN)"

# C++ 封装库测试
wrapper-test: $(WRAPPER_A) $(LIBTCC_A) wrapper_test.cpp
	$(CXX) $(CXXFLAGS) -I$(TCC_SRC_DIR) wrapper_test.cpp $(WRAPPER_A) $(LIBTCC_A) -o $(WRAPPER_TEST) -ldl -lpthread
	@echo "✓ Wrapper test binary: $(WRAPPER_TEST)"
	@echo ""
	@echo "Run test:"
	@echo "  ./$(WRAPPER_TEST)"

# 示例程序
example: $(WRAPPER_A) $(LIBTCC_A) example.cpp
	$(CXX) $(CXXFLAGS) -I$(TCC_SRC_DIR) example.cpp $(WRAPPER_A) $(LIBTCC_A) -o $(EXAMPLE) -ldl -lpthread
	@echo "✓ Example binary: $(EXAMPLE)"
	@echo ""
	@echo "Usage:"
	@echo "  ./$(EXAMPLE) source.c output.a"

clean:
	rm -f *.o $(LIBTCC_A) $(LIBTCC_SO) $(WRAPPER_A) $(WRAPPER_SO) $(TEST_BIN) $(WRAPPER_TEST) $(EXAMPLE)

install: $(LIBTCC_A) $(LIBTCC_SO) $(WRAPPER_A) $(WRAPPER_SO)
	@echo "Installing to /usr/local/lib..."
	sudo cp $(LIBTCC_A) /usr/local/lib/
	sudo cp $(LIBTCC_SO) /usr/local/lib/
	sudo cp $(WRAPPER_A) /usr/local/lib/
	sudo cp $(WRAPPER_SO) /usr/local/lib/
	sudo cp tcc_wrapper.h /usr/local/include/
	sudo ldconfig
	@echo "✓ Installed"
