//#define _GNU_SOURCE
#include <iostream>
#include <cstring>
#include <dlfcn.h>
#include <cassert>
#include <memory>

// 定义 diff_context_t（必须与 cosim.h 中完全一致）
constexpr size_t VLEN = 256;
constexpr size_t VENUM64 = VLEN / 64; 
constexpr size_t VENUM32 = VLEN / 32; 
constexpr size_t VENUM16 = VLEN / 16; 
constexpr size_t VENUM8  = VLEN / 8; 

struct diff_context_t {
    uint64_t pc;
    uint64_t gpr[32];
    uint64_t fpr[32];
    union VecReg {
        uint64_t _64[VENUM64];
        uint32_t _32[VENUM32 * 2];
        uint16_t _16[VENUM16 * 4];
        uint8_t  _8[VENUM8 * 8];
    } vr[32];
};

// 方向常量（与 cosim.h 一致）
constexpr int ENV_TO_REF = 1;
constexpr int REF_TO_ENV = 0;

// 声明外部 C 接口函数指针类型（必须用 extern "C" 链接约定）
extern "C" {
    using ref_init_t      = void(*)(int);
    using ref_close_t     = void(*)();
    using ref_exec_t      = void(*)(uint64_t);
    using cosim_regcpy_t  = void(*)(diff_context_t*, int, int);
    using cosim_memcpy_t  = void(*)(uint64_t, void*, size_t, int);
}

// RAII 封装 dlopen 句柄
class DLHandle {
public:
    explicit DLHandle(const char* path) {
        handle_ = dlopen(path, RTLD_LAZY);
        if (!handle_) {
            throw std::runtime_error("dlopen failed: " + std::string(dlerror()));
        }
    }

    ~DLHandle() {
        if (handle_) dlclose(handle_);
    }

    void* get_symbol(const char* name) {
        dlerror(); // Clear any existing error
        void* sym = dlsym(handle_, name);
        const char* err = dlerror();
        if (err) {
            throw std::runtime_error("dlsym failed for '" + std::string(name) + "': " + std::string(err));
        }
        return sym;
    }

    DLHandle(const DLHandle&) = delete;
    DLHandle& operator=(const DLHandle&) = delete;

private:
    void* handle_ = nullptr;
};

int main() {
    try {
        // 1. 加载动态库
        DLHandle lib("./build/riscv64-spike-so");

        // 2. 获取函数指针
        auto ref_init      = reinterpret_cast<ref_init_t>(lib.get_symbol("ref_init"));
        auto ref_close     = reinterpret_cast<ref_close_t>(lib.get_symbol("ref_close"));
        auto ref_exec      = reinterpret_cast<ref_exec_t>(lib.get_symbol("ref_exec"));
        auto cosim_regcpy  = reinterpret_cast<cosim_regcpy_t>(lib.get_symbol("cosim_regcpy"));
        auto cosim_memcpy  = reinterpret_cast<cosim_memcpy_t>(lib.get_symbol("cosim_memcpy"));

        // 3. 初始化参考模型
        std::cout << "Initializing CosimRef...\n";
        ref_init(0);

        // 4. 设置初始状态: x1 = 50, pc = 0x80000000
        diff_context_t ctx{};
        ctx.pc = 0x80000000ULL;
        ctx.gpr[1] = 50; // x1 = 50

        std::cout << "Setting initial state: x1 = " << ctx.gpr[1]
                  << ", pc = 0x" << std::hex << ctx.pc << std::dec << "\n";
        cosim_regcpy(&ctx, ENV_TO_REF, 0); // on_demand = false

        // 5. 写入指令: addi x2, x1, 30  => 机器码: 0x01e08113
        uint32_t inst = 0x01e08113; // addi x2, x1, 30
        std::cout << "Writing instruction at 0x" << std::hex << ctx.pc
                  << ": 0x" << inst << std::dec << "\n";
        cosim_memcpy(ctx.pc, &inst, sizeof(inst), ENV_TO_REF);

        // 6. 执行 1 条指令
        std::cout << "Executing 1 instruction...\n";
        ref_exec(1);

        // 7. 获取执行后状态
        cosim_regcpy(&ctx, REF_TO_ENV, 0);

        // 8. 验证结果
        std::cout << "After execution:\n";
        std::cout << "  x1 = " << ctx.gpr[1] << "\n";
        std::cout << "  x2 = " << ctx.gpr[2] << "\n";
        std::cout << "  pc = 0x" << std::hex << ctx.pc << std::dec << "\n";

        assert(ctx.gpr[2] == 80);         // 50 + 30 = 80
        assert(ctx.pc == 0x80000004ULL);  // PC += 4

        std::cout << "✅ C++ dlopen test passed!\n";

        // 9. 清理（ref_close 会被调用，DLHandle 析构自动 dlclose）
        ref_close();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
