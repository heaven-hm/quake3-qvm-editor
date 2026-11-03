#include "qvm/format/QvmInstruction.hpp"
#include <iomanip>
#include <sstream>

namespace qvm {

std::string_view getOpcodeName(Opcode op) noexcept {
    switch (op) {
        case Opcode::Undef:      return "undef";
        case Opcode::Ignore:     return "ignore";
        case Opcode::Break:      return "break";
        case Opcode::Enter:      return "enter";
        case Opcode::Leave:      return "leave";
        case Opcode::Call:       return "call";
        case Opcode::Push:       return "push";
        case Opcode::Pop:        return "pop";
        case Opcode::Const:      return "const";
        case Opcode::Local:      return "local";
        case Opcode::Jump:       return "jump";
        case Opcode::Eq:         return "eq";
        case Opcode::Ne:         return "ne";
        case Opcode::LtI:        return "lti";
        case Opcode::LeI:        return "lei";
        case Opcode::GtI:        return "gti";
        case Opcode::GeI:        return "gei";
        case Opcode::LtU:        return "ltu";
        case Opcode::LeU:        return "leu";
        case Opcode::GtU:        return "gtu";
        case Opcode::GeU:        return "geu";
        case Opcode::EqF:        return "eqf";
        case Opcode::NeF:        return "nef";
        case Opcode::LtF:        return "ltf";
        case Opcode::LeF:        return "lef";
        case Opcode::GtF:        return "gtf";
        case Opcode::GeF:        return "gef";
        case Opcode::Load1:      return "load1";
        case Opcode::Load2:      return "load2";
        case Opcode::Load4:      return "load4";
        case Opcode::Store1:     return "store1";
        case Opcode::Store2:     return "store2";
        case Opcode::Store4:     return "store4";
        case Opcode::Arg:        return "arg";
        case Opcode::BlockCopy:  return "block_copy";
        case Opcode::Sex8:       return "sex8";
        case Opcode::Sex16:      return "sex16";
        case Opcode::NegI:       return "negi";
        case Opcode::Add:        return "add";
        case Opcode::Sub:        return "sub";
        case Opcode::DivI:       return "divi";
        case Opcode::DivU:       return "divu";
        case Opcode::ModI:       return "modi";
        case Opcode::ModU:       return "modu";
        case Opcode::MulI:       return "muli";
        case Opcode::MulU:       return "mulu";
        case Opcode::BAnd:       return "band";
        case Opcode::BOr:        return "bor";
        case Opcode::BXor:       return "bxor";
        case Opcode::BCom:       return "bcom";
        case Opcode::Lsh:        return "lsh";
        case Opcode::RshI:       return "rshi";
        case Opcode::RshU:       return "rshu";
        case Opcode::NegF:       return "negf";
        case Opcode::AddF:       return "addf";
        case Opcode::SubF:       return "subf";
        case Opcode::DivF:       return "divf";
        case Opcode::MulF:       return "mulf";
        case Opcode::CvIF:       return "cvif";
        case Opcode::CvFI:       return "cvfi";
        default:                 return "unknown";
    }
}

uint8_t getOpcodeParamSize(Opcode op) noexcept {
    switch (op) {
        case Opcode::Enter:
        case Opcode::Leave:
        case Opcode::Const:
        case Opcode::Local:
        case Opcode::BlockCopy:
        case Opcode::Eq:
        case Opcode::Ne:
        case Opcode::LtI:
        case Opcode::LeI:
        case Opcode::GtI:
        case Opcode::GeI:
        case Opcode::LtU:
        case Opcode::LeU:
        case Opcode::GtU:
        case Opcode::GeU:
        case Opcode::EqF:
        case Opcode::NeF:
        case Opcode::LtF:
        case Opcode::LeF:
        case Opcode::GtF:
        case Opcode::GeF:
            return 4;
        case Opcode::Arg:
            return 1;
        default:
            return 0;
    }
}

bool isConditionalBranch(Opcode op) noexcept {
    switch (op) {
        case Opcode::Eq:
        case Opcode::Ne:
        case Opcode::LtI:
        case Opcode::LeI:
        case Opcode::GtI:
        case Opcode::GeI:
        case Opcode::LtU:
        case Opcode::LeU:
        case Opcode::GtU:
        case Opcode::GeU:
        case Opcode::EqF:
        case Opcode::NeF:
        case Opcode::LtF:
        case Opcode::LeF:
        case Opcode::GtF:
        case Opcode::GeF:
            return true;
        default:
            return false;
    }
}

bool isUnconditionalJump(Opcode op) noexcept {
    return op == Opcode::Jump;
}

bool isBranchOrJump(Opcode op) noexcept {
    return isConditionalBranch(op) || isUnconditionalJump(op);
}

std::string_view QvmInstruction::opcodeName() const noexcept {
    return getOpcodeName(opcode);
}

std::string QvmInstruction::formatAssembly() const {
    std::stringstream ss;
    ss << std::setw(8) << std::setfill('0') << std::hex << offset << "  ";
    ss << std::left << std::setw(12) << std::setfill(' ') << opcodeName();

    if (operandSize > 0) {
        ss << " " << std::right << std::setw(10) << std::dec << operand;
    } else {
        ss << "           ";
    }

    if (!comment.empty()) {
        ss << "  ; " << comment;
    }

    return ss.str();
}

} // namespace qvm
