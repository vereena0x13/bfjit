#define VSTD_IMPL
#include "vstd.hpp"
#undef unused // @Hack


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra-semi"
#include "asmjit/x86.h"
#pragma GCC diagnostic pop

using namespace asmjit;
using namespace x86;




typedef s32 (*Fn)(void);


s32 main(s32 argc, cstr *argv) {
    JitRuntime rt;
    

    CodeHolder code;
    code.init(rt.environment(), rt.cpuFeatures());


    Compiler cc(&code);
    
    cc.addFunc(FuncSignature::build<s32>());
    auto reg = cc.newInt32();
    cc.mov(reg, 42);
    cc.ret(reg);
    cc.endFunc();

    cc.finalize();


    Fn f;
    auto err = rt.add(&f, &code);
    if(err) {
        printf("JIT failed\n");
        return 1;
    }


    s32 result = f();
    printf("f() = %d\n", result);


    return 0;
}