#pragma once

#include "FEXCore/Utils/EnumUtils.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Emitter.h"
#include "Interface/Core/ArchHelpers/CodeEmitter/Registers.h"

#include "Interface/Core/Dispatcher/Dispatcher.h"
#include "Interface/Core/ObjectCache/Relocations.h"

#include <aarch64/assembler-aarch64.h>
#include <aarch64/constants-aarch64.h>
#include <aarch64/cpu-aarch64.h>
#include <aarch64/operands-aarch64.h>
#include <platform-vixl.h>
#ifdef VIXL_DISASSEMBLER
#include <aarch64/disasm-aarch64.h>
#endif
#ifdef VIXL_SIMULATOR
#include <aarch64/simulator-aarch64.h>
#include <aarch64/simulator-constants-aarch64.h>
#endif

#include <FEXCore/Config/Config.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <span>

namespace FEXCore::CPU {
// Contains the address to the currently available CPU state
constexpr auto STATE = FEXCore::ARMEmitter::XReg::x28;

// GPR temporaries. Only x3 can be used across spill boundaries
// so if these ever need to change, be very careful about that.
constexpr auto TMP1 = FEXCore::ARMEmitter::XReg::x0;
constexpr auto TMP2 = FEXCore::ARMEmitter::XReg::x1;
constexpr auto TMP3 = FEXCore::ARMEmitter::XReg::x2;
constexpr auto TMP4 = FEXCore::ARMEmitter::XReg::x3;

// Vector temporaries
constexpr auto VTMP1 = FEXCore::ARMEmitter::VReg::v0;
constexpr auto VTMP2 = FEXCore::ARMEmitter::VReg::v1;
constexpr auto VTMP3 = FEXCore::ARMEmitter::VReg::v2;
constexpr auto VTMP4 = FEXCore::ARMEmitter::VReg::v3;

// Predicate register temporaries (used when AVX support is enabled)
// PRED_TMP_16B indicates a predicate register that indicates the first 16 bytes set to 1.
// PRED_TMP_32B indicates a predicate register that indicates the first 32 bytes set to 1.
constexpr FEXCore::ARMEmitter::PRegister PRED_TMP_16B = FEXCore::ARMEmitter::PReg::p6;
constexpr FEXCore::ARMEmitter::PRegister PRED_TMP_32B = FEXCore::ARMEmitter::PReg::p7;

// This class contains common emitter utility functions that can
// be used by both Arm64 JIT and ARM64 Dispatcher
class Arm64Emitter : public FEXCore::ARMEmitter::Emitter {
protected:
  Arm64Emitter(FEXCore::Context::ContextImpl *ctx, size_t size);
  ~Arm64Emitter();

  FEXCore::Context::ContextImpl *EmitterCTX;
  vixl::aarch64::CPU CPU;

  std::span<const FEXCore::ARMEmitter::Register> ConfiguredDynamicRegisterBase{};
  std::span<const FEXCore::ARMEmitter::Register> StaticRegisters{};
  std::span<const FEXCore::ARMEmitter::Register> GeneralRegisters{};
  std::span<const std::pair<FEXCore::ARMEmitter::Register, FEXCore::ARMEmitter::Register>> GeneralPairRegisters{};
  std::span<const FEXCore::ARMEmitter::VRegister> StaticFPRegisters{};
  std::span<const FEXCore::ARMEmitter::VRegister> GeneralFPRegisters{};

  /**
   * @name Register Allocation
   * @{ */
  constexpr static uint32_t RegisterClasses = 6;

  constexpr static uint64_t GPRBase = (0ULL << 32);
  constexpr static uint64_t FPRBase = (1ULL << 32);
  constexpr static uint64_t GPRPairBase = (2ULL << 32);

  /**  @} */

  constexpr static uint8_t RA_32 = 0;
  constexpr static uint8_t RA_64 = 1;
  constexpr static uint8_t RA_FPR = 2;

  void LoadConstant(FEXCore::ARMEmitter::Size s, FEXCore::ARMEmitter::Register Reg, uint64_t Constant, bool NOPPad = false);


  // NOTE: These functions WILL clobber the register TMP4 if AVX support is enabled
  //       and FPRs are being spilled or filled. If only GPRs are spilled/filled, then
  //       TMP4 is left alone.
  void SpillStaticRegs(FEXCore::ARMEmitter::Register TmpReg, bool FPRs = true, uint32_t GPRSpillMask = ~0U, uint32_t FPRSpillMask = ~0U);
  void FillStaticRegs(bool FPRs = true, uint32_t GPRFillMask = ~0U, uint32_t FPRFillMask = ~0U);

  // Register 0-18 + 29 + 30 are caller saved
  static constexpr uint32_t CALLER_GPR_MASK = 0b0110'0000'0000'0111'1111'1111'1111'1111U;

  // This isn't technically true because the lower 64-bits of v8..v15 are callee saved
  // We can't guarantee only the lower 64bits are used so flush everything
  static constexpr uint32_t CALLER_FPR_MASK = ~0U;

  void PushDynamicRegsAndLR(FEXCore::ARMEmitter::Register TmpReg);
  void PopDynamicRegsAndLR();

  void PushCalleeSavedRegisters();
  void PopCalleeSavedRegisters();

  void Align16B();

#ifdef VIXL_SIMULATOR
  // Generates a vixl simulator runtime call.
  //
  // This matches behaviour of vixl's macro assembler, but we need to reimplement it since we aren't using the macro assembler.
  // This isn't too complex with how vixl emits this.
  //
  // Emit:
  // 1) hlt(kRuntimeCallOpcode)
  // 2) Simulator wrapper handler
  // 3) Function to call
  // 4) Style of the function call (Call versus tail-call)

  template<typename R, typename... P>
  void GenerateRuntimeCall(R (*Function)(P...)) {
    uintptr_t SimulatorWrapperAddress = reinterpret_cast<uintptr_t>(
      &(vixl::aarch64::Simulator::RuntimeCallStructHelper<R, P...>::Wrapper));

    uintptr_t FunctionAddress = reinterpret_cast<uintptr_t>(Function);

    hlt(vixl::aarch64::kRuntimeCallOpcode);

    // Simulator wrapper address pointer.
    dc64(SimulatorWrapperAddress);

    // Runtime function address to call
    dc64(FunctionAddress);

    // Call type
    dc32(vixl::aarch64::kCallRuntime);
  }

  template<typename R, typename... P>
  void GenerateIndirectRuntimeCall(ARMEmitter::Register Reg) {
    uintptr_t SimulatorWrapperAddress = reinterpret_cast<uintptr_t>(
      &(vixl::aarch64::Simulator::RuntimeCallStructHelper<R, P...>::Wrapper));

    hlt(vixl::aarch64::kIndirectRuntimeCallOpcode);

    // Simulator wrapper address pointer.
    dc64(SimulatorWrapperAddress);

    // Register that contains the function to call
    dc32(Reg.Idx());

    // Call type
    dc32(vixl::aarch64::kCallRuntime);
  }

  template<>
  void GenerateIndirectRuntimeCall<float, __uint128_t>(ARMEmitter::Register Reg) {
    uintptr_t SimulatorWrapperAddress = reinterpret_cast<uintptr_t>(
      &(vixl::aarch64::Simulator::RuntimeCallStructHelper<float, __uint128_t>::Wrapper));

    hlt(vixl::aarch64::kIndirectRuntimeCallOpcode);

    // Simulator wrapper address pointer.
    dc64(SimulatorWrapperAddress);

    // Register that contains the function to call
    dc32(Reg.Idx());

    // Call type
    dc32(vixl::aarch64::kCallRuntime);
  }

#endif
#ifdef VIXL_DISASSEMBLER
  vixl::aarch64::PrintDisassembler Disasm {stderr};
#endif
  FEX_CONFIG_OPT(StaticRegisterAllocation, SRA);
};

}
