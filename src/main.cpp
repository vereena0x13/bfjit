#define VSTD_IMPL
#include "vstd.hpp"
#undef unused // @Hack


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra-semi"
#include "asmjit/x86.h"
#pragma GCC diagnostic pop

using namespace asmjit;
using namespace x86;


#include <immintrin.h>



enum OpCode : u8 {
    INC,
    DEC,
    RIGHT,
    LEFT,
    READ,
    WRITE,
    OPEN,
    CLOSE,
    SET,

    INVALID,
};

struct Insn {
    OpCode op;
    u32 operand;

    Insn(OpCode _op, u32 _operand = 0) : op(_op), operand(_operand) {}
};

static_assert(sizeof(Insn) == 8);


Array<Insn> parse_brainfuck(str code) {
    Array<Insn> insns;
    Array<u32> stack;

    for(u32 i = 0; i < strsz(code); i++) {
        char c = code[i];
        if(c == '+' || c == '-' || c == '>' || c == '<' || c == ',' || c == '.') {
            u32 count = 1;

            while(i + 1 < strsz(code) && code[i + 1] == c) {
                i++;
                count++;
            }

            OpCode op = INVALID;
            if(c == '+') op = INC;
            if(c == '-') op = DEC;
            if(c == '>') op = RIGHT;
            if(c == '<') op = LEFT;
            if(c == ',') op = READ;
            if(c == '.') op = WRITE;
            assert(op != INVALID);

            insns.push(Insn(op, count));
        } else if(c == '[') {
            if(i + 2 < strsz(code) && code[i + 1] == '-' && code[i + 2] == ']') {
                insns.push(Insn(SET, 0));
                i += 2;
            } else {
                u32 idx = insns.push(Insn(OPEN));
                stack.push(idx);
            }
        } else if(c == ']') {
            u32 j = stack.pop();
            insns[j].operand = insns.count + 1;
            insns.push(Insn(CLOSE, j + 1));
        } else {
            // ignore
        }
    }

    stack.free();

    return insns;
}


typedef s32 (*BfFn)(u8*);

BfFn compile(JitRuntime &rt, Array<Insn> const& ir, bool dual_cmp) {
    CodeHolder code;
    code.init(rt.environment(), rt.cpuFeatures());

#if 0
    FileLogger logger(stdout);
    FormatOptions fopts;
    fopts.setIndentation(FormatIndentationGroup::kCode, 4);
    logger.setOptions(fopts);
    code.setLogger(&logger);
#endif

    Compiler cc(&code);

    auto func = cc.addFunc(FuncSignature::build<s32, u8*>());
    
    auto dp = cc.newUInt32();
    cc.xor_(dp, dp);

    auto tapePtr = cc.newIntPtr();
    func->setArg(0, tapePtr);

    auto tmp = cc.newUInt8();

    Mem tapeIndex(tapePtr, dp, 0, 0, 1);

    auto putcharSig = FuncSignature::build<s32, s32>();
    auto getcharSig = FuncSignature::build<s32>();

    Array<Label> labels; // TODO: this is lazy...
    for(auto const& insn : ir) {
        switch(insn.op) {
            case INC: {
                cc.add(tapeIndex, Imm(insn.operand));
                break;
            }
            case DEC: {
                cc.sub(tapeIndex, Imm(insn.operand));
                break;
            }
            case RIGHT: {
                cc.add(dp, Imm(insn.operand));
                break;
            }
            case LEFT: {
                cc.sub(dp, Imm(insn.operand));
                break;
            }
            case READ: {
                for(u32 i = 0; i < insn.operand; i++) {
                    InvokeNode *inv;
                    cc.invoke(&inv, Imm(getchar), getcharSig);
                    inv->setRet(0, tmp);
                    cc.mov(tapeIndex, tmp);
                }
                break;
            }
            case WRITE: {
                cc.mov(tmp, tapeIndex);
                for(u32 i = 0; i < insn.operand; i++) {
                    InvokeNode *inv;
                    cc.invoke(&inv, Imm(putchar), putcharSig);
                    inv->setArg(0, tmp);
                }
                break;
            }
            case OPEN: {
                auto lsl = cc.newLabel();
                auto lel = cc.newLabel();
                labels.push(lsl);
                labels.push(lel);
                
                if(dual_cmp) {
                    cc.mov(tmp, tapeIndex);
                    cc.cmp(tmp, 0);
                    cc.je(lel);
                    cc.bind(lsl);
                } else {
                    cc.bind(lsl);
                    cc.mov(tmp, tapeIndex);
                    cc.cmp(tmp, 0);
                    cc.je(lel);
                }
                break;
            }
            case CLOSE: {
                auto lel = labels.pop();
                auto lsl = labels.pop();

                if(dual_cmp) {
                    cc.mov(tmp, tapeIndex);
                    cc.cmp(tmp, 0);
                    cc.jne(lsl);
                    cc.bind(lel);
                } else {
                    cc.jmp(lsl);
                    cc.bind(lel);
                }
                break;
            }
            case SET: {
                cc.mov(tapeIndex, Imm(insn.operand));
                break;
            }
            default: {
                panic("invalid opcode %d\n", insn.op);
                break;
            }
        }
    }
    
    assert(labels.count == 0);
    labels.free();

    auto ret = cc.newInt32();
    cc.mov(ret, 42);
    cc.ret(ret);
    cc.endFunc();

    cc.finalize();


    BfFn fn;
    auto err = rt.add(&fn, &code);
    if(err) { 
        panic("JIT failed\n");
        return NULL;
    }

    return fn;
}

u64 do_test(BfFn fn, u32 N) {
    Static_Array<u8, 1024 * 128> tape;

    u64 total = 0;
    for(u32 i = 0; i < N; i++) {
        memset(tape.data, 0, tape.size);
        u64 start = __rdtsc();
        assert(fn(tape.data) == 42);
        u64 end = __rdtsc();
        total += (start - end);
    }

    return total;
}

s32 main(s32 argc, cstr *argv) {
    if(argc < 2) {
        printf("Usage: bf <file>\n");
        return 1;
    }

    
    auto code = read_entire_file(argv[1]);
    auto ir = parse_brainfuck(code);
    

    JitRuntime rt;

    auto fn = compile(rt, ir, true);

    Static_Array<u8, 1024 * 128> tape;
    memset(tape.data, 0, tape.size);
    assert(fn(tape.data) == 42);


    // auto f0 = compile(rt, ir, true);
    // printf("\n");
    // auto f1 = compile(rt, ir, false);


    // Static_Array<u8, 1024 * 128> tape;
    
    // memset(tape.data, 0, tape.size);
    // assert(f0(tape.data) == 42);
    
    // memset(tape.data, 0, tape.size);
    // assert(f1(tape.data) == 42);


    // constexpr u32 N = 10;
    // u64 t0 = do_test(f0, N);
    // u64 t1 = do_test(f1, N);
    // printf("dual:   %llu\nsingle: %llu\n", t0/N, t1/N);


    return 0;
}