#pragma once

#include <FEXCore/Debug/X86Tables.h>
#include <cstdint>
#include <utility>
#include <set>
#include <vector>

namespace FEXCore::Context {
struct Context;
}

namespace FEXCore::Frontend {
class Decoder final {
public:
  Decoder(FEXCore::Context::Context *ctx);
  bool DecodeInstructionsInBlock(uint8_t const* InstStream, uint64_t PC);

  std::pair<std::vector<FEXCore::X86Tables::DecodedInst>*, size_t> const GetDecodedInsts() {
    return std::make_pair(&DecodedBuffer, DecodedSize);
  }
  std::set<uint64_t> JumpTargets;

private:
  FEXCore::Context::Context *CTX;

  bool DecodeInstruction(uint64_t PC);

  bool BlockEndCanContinuePast();
  bool BranchTargetInMultiblockRange();

  void DecodeModRM(uint8_t *Displacement, FEXCore::X86Tables::ModRMDecoded ModRM);
  bool DecodeSIB(uint8_t *Displacement, FEXCore::X86Tables::ModRMDecoded ModRM);
  uint8_t ReadByte();
  uint8_t PeekByte(uint8_t Offset);
  uint64_t ReadData(uint8_t Size);
  void SkipBytes(uint8_t Size) { InstructionSize += Size; }
  bool NormalOp(FEXCore::X86Tables::X86InstInfo const *Info, uint16_t Op);
  bool NormalOpHeader(FEXCore::X86Tables::X86InstInfo const *Info, uint16_t Op);

  static constexpr size_t DefaultDecodedBufferSize = 0x10000;
  std::vector<FEXCore::X86Tables::DecodedInst> DecodedBuffer;
  size_t DecodedSize {};

  uint8_t const *InstStream;

  static constexpr size_t MAX_INST_SIZE = 15;
  uint8_t InstructionSize;
  std::array<uint8_t, MAX_INST_SIZE> Instruction;
  FEXCore::X86Tables::DecodedInst *DecodeInst;

  // This is for multiblock data tracking
  bool SymbolAvailable {false};
  uint64_t EntryPoint {};
  uint64_t MaxCondBranchForward {};
  uint64_t MaxCondBranchBackwards {~0ULL};
  uint64_t SymbolMaxAddress {};
  uint64_t SymbolMinAddress {~0ULL};

};
}
