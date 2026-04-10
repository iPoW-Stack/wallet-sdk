CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall \
            -I./uWebSockets-20.62.0/src \
            -I./uSockets-0.8.8/src \
            -DUWS_NO_ZLIB \
            -DLIBUS_USE_OPENSSL

# macOS: openssl via homebrew
OPENSSL_PREFIX := $(shell brew --prefix openssl 2>/dev/null || echo /usr/local)
CXXFLAGS += -I$(OPENSSL_PREFIX)/include
LDFLAGS  := -L$(OPENSSL_PREFIX)/lib -lssl -lcrypto -lpthread

# uSockets C sources
USOCKETS_SRC := \
    uSockets-0.8.8/src/bsd.c \
    uSockets-0.8.8/src/loop.c \
    uSockets-0.8.8/src/context.c \
    uSockets-0.8.8/src/socket.c \
    uSockets-0.8.8/src/udp.c \
    uSockets-0.8.8/src/crypto/openssl.c \
    uSockets-0.8.8/src/crypto/sni_tree.cpp \
    uSockets-0.8.8/src/eventing/epoll_kqueue.c

USOCKETS_OBJ := $(USOCKETS_SRC:.c=.o)
USOCKETS_OBJ := $(USOCKETS_OBJ:.cpp=.o)

TARGET := server

.PHONY: all clean certs

all: $(TARGET)

# Compile uSockets C files
%.o: %.c
	$(CC) -c $(CFLAGS) -DLIBUS_USE_OPENSSL -I$(OPENSSL_PREFIX)/include \
	    -I./uSockets-0.8.8/src $< -o $@

uSockets-0.8.8/src/crypto/sni_tree.o: uSockets-0.8.8/src/crypto/sni_tree.cpp
	$(CXX) -c -std=c++17 -DLIBUS_USE_OPENSSL -I$(OPENSSL_PREFIX)/include \
	    -I./uSockets-0.8.8/src $< -o $@

$(TARGET): server.cpp $(USOCKETS_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(USOCKETS_OBJ)

certs:
	mkdir -p certs
	openssl req -x509 -newkey rsa:4096 -keyout certs/key.pem -out certs/cert.pem \
	    -days 3650 -nodes \
	    -subj "/C=CN/ST=Beijing/L=Beijing/O=Dev/CN=localhost"
