# ===== 使用方式 =====
# make tests               # 編譯所有測試
# ./build/test_icache_hit  # 跑特定測試
# make run-icache_hit      # 編譯 + 跑 (一行搞定)
# make clean               # 全部清掉重來
# ====================

CXX := g++
CXXFLAGS := -std=c++17 -O0 -g -Wall -Iinclude -Isrc
LDFLAGS := -lsystemc

# 自動找 src/*.cpp
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)

# 自動找所有 test/test_*.cpp，每個編成 build/test_*
TESTS := $(wildcard test/test_*.cpp)
TEST_BINS := $(TESTS:test/%.cpp=build/%)

.PHONY: all clean tests

all: tests

tests: $(TEST_BINS)

# 規則: src/*.cpp -> build/*.o
build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 規則: test/test_xxx.cpp + 所有 src/*.o -> build/test_xxx
build/test_%: test/test_%.cpp $(OBJ)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

clean:
	rm -rf build

# 執行特定 test 的方便指令
run-%: build/test_%
	./build/test_$*