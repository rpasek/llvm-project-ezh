//===- EZH.cpp ------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "RelocScan.h"
#include "Symbols.h"
#include "Target.h"
#include "lld/Common/ErrorHandler.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

class EZH final : public TargetInfo {
public:
  EZH(Ctx &ctx) : TargetInfo(ctx) { needsThunks = true; }
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  bool needsThunk(RelExpr expr, RelType type, const InputFile *file,
                  uint64_t branchAddr, const Symbol &s,
                  int64_t a) const override;
  bool inBranchRange(RelType type, uint64_t src, uint64_t dst) const override;
  template <class ELFT, class RelTy>
  void scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels);
  void scanSection(InputSectionBase &sec) override {
    elf::scanSection1<EZH, ELF32LE>(*this, sec);
  }
};

} // namespace

bool EZH::needsThunk(RelExpr expr, RelType type, const InputFile *file,
                     uint64_t branchAddr, const Symbol &s, int64_t a) const {
  if (type == R_EZH_21) {
    uint64_t dst = s.getVA(ctx, a);
    return !inBranchRange(type, branchAddr, dst);
  }
  return false;
}

bool EZH::inBranchRange(RelType type, uint64_t src, uint64_t dst) const {
  if (type == R_EZH_21) {
    return (src & 0xff800000) == (dst & 0xff800000);
  }
  return true;
}

RelExpr EZH::getRelExpr(RelType type, const Symbol &s,
                        const uint8_t *loc) const {
  switch (type) {
  case R_EZH_11:
  case R_EZH_12:
  case R_EZH_30:
  case R_EZH_32:
    return R_ABS;
  case R_EZH_21:
    return R_PC;
  case R_EZH_NONE:
    return R_NONE;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unknown relocation (" << type
             << ") against symbol " << &s;
    return R_NONE;
  }
}

void EZH::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_EZH_NONE:
    break;
  case R_EZH_11: {
    uint32_t inst = read32le(loc);
    uint32_t hi = (val >> 20) & 0x7ff;
    inst = (inst & ~(0x7ff << 20)) | (hi << 20);
    write32le(loc, inst);
    break;
  }
  case R_EZH_12: {
    uint32_t inst = read32le(loc);
    uint32_t lo = val & 0xfff;
    inst = (inst & ~(0xfff << 20)) | (lo << 20);
    write32le(loc, inst);
    break;
  }
  case R_EZH_21: {
    uint64_t target = rel.sym->getVA(ctx, rel.addend);
    uint64_t pc = target - val;
    if ((pc & 0xff800000) != (target & 0xff800000)) {
      reportRangeError(ctx, loc, rel, Twine(target), pc & 0xff800000,
                       (pc & 0xff800000) + 0x7fffff);
    }
    uint32_t inst = read32le(loc);
    inst = (inst & 0x000007ff) | (((target >> 2) & 0x1fffff) << 11);
    write32le(loc, inst);
    break;
  }
  case R_EZH_30: {
    uint32_t inst = read32le(loc);

    // E_GOSUB (Opcode 0x03) embeds the absolute physical address into the
    // instruction. The hardware masks out the lowest 2 bits (which contain the
    // opcode) when branching.
    inst = (inst & 0x00000003) | (val & 0xfffffffc);
    write32le(loc, inst);
    break;
  }
  case R_EZH_32:
    write32le(loc, val);
    break;
  default:
    llvm_unreachable("unknown relocation");
  }
}
template <class ELFT, class RelTy>
void EZH::scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels) {
  RelocScan rs(ctx, &sec);
  sec.relocations.reserve(rels.size());
  for (auto it = rels.begin(); it != rels.end(); ++it) {
    RelType type = it->getType(false);
    if (type == R_EZH_NONE)
      continue;
    rs.scan<ELFT, RelTy>(it, type, rs.getAddend<ELFT>(*it, type));
  }
}

namespace lld {
namespace elf {
void setEZHTargetInfo(Ctx &ctx) { ctx.target.reset(new EZH(ctx)); }
} // namespace elf
} // namespace lld
