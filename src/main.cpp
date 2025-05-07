#define VSTD_IMPL
#include "vstd.hpp"
#undef unused // @Hack


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra-semi"
#include "asmjit/x86.h"
#pragma GCC diagnostic pop

using namespace asmjit;
using namespace x86;




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


s32 main(s32 argc, cstr *argv) {
    if(argc < 2) {
        printf("Usage: bf <file>\n");
        return 1;
    }


    auto code = read_entire_file(argv[1]);
    auto ir = parse_brainfuck(code);


    Static_Array<u8, 1024 * 128> tape;
    for(u32 i = 0; i < tape.size; i++) tape[i] = 0;

    u32 ip = 0;
    u32 dp = 0;


    while(ip < ir.count) {
        auto const& insn = ir[ip];
        switch(insn.op) {
            case INC: {
                tape[dp] = (tape[dp] + insn.operand) & 0xFF;
                ip++;
                break;
            }
            case DEC: {
                tape[dp] = (tape[dp] - insn.operand) & 0xFF;
                ip++;
                break;
            }
            case RIGHT: {
                dp += insn.operand;
                ip++;
                break;
            }
            case LEFT: {
                dp -= insn.operand;
                ip++;
                break;
            }
            case READ: {
                // TODO
                ip++;
                break;
            }
            case WRITE: {
                for(u32 i = 0; i < insn.operand; i++) {
                    printf("%c", tape[dp]);
                    fflush(stdout);
                }
                ip++;
                break;
            }
            case OPEN: {
                if(tape[dp] == 0) {
                    ip = insn.operand;
                } else {
                    ip++;
                }
                break;
            }
            case CLOSE: {
                if(tape[dp] != 0) {
                    ip = insn.operand;
                } else {
                    ip++;
                }
                break;
            }
            case SET: {
                tape[dp] = insn.operand & 0xFF;
                ip++;
                break;
            }
            case INVALID: {
                assert(false);
            }
            default: {
                assert(false);
            }
        }
    }


    return 0;
}