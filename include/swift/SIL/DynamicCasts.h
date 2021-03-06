//===--- DynamicCasts.h - SIL dynamic-cast utilities ------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file provides basic utilities for working with subtyping
// relationships.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SIL_DYNAMICCASTS_H
#define SWIFT_SIL_DYNAMICCASTS_H

#include "swift/AST/Module.h"
#include "swift/Basic/ProfileCounter.h"
#include "swift/SIL/SILInstruction.h"
#include "swift/SIL/SILModule.h"
#include "swift/SIL/SILValue.h"

namespace swift {

class CanType;
class ModuleDecl;
class SILBuilder;
class SILLocation;
class SILModule;
class SILType;
enum class CastConsumptionKind : unsigned char;
struct SILDynamicCastInst;

enum class DynamicCastFeasibility {
  /// The cast will always succeed.
  WillSucceed,

  /// The cast can succeed for some values.
  MaySucceed,

  /// The cast cannot succeed.
  WillFail,
};

static inline DynamicCastFeasibility
atWorst(DynamicCastFeasibility feasibility, DynamicCastFeasibility worstCase) {
  return (feasibility > worstCase ? worstCase : feasibility);
}

static inline DynamicCastFeasibility
atBest(DynamicCastFeasibility feasibility, DynamicCastFeasibility bestCase) {
  return (feasibility < bestCase ? bestCase : feasibility);
}

/// Classify the feasibility of a dynamic cast.  The source and target
/// types should be unlowered formal types.
DynamicCastFeasibility classifyDynamicCast(
    ModuleDecl *context,
    CanType sourceType, CanType targetType,
    bool isSourceTypeExact = false,
    bool isWholdModuleOpts = false);

SILValue emitSuccessfulScalarUnconditionalCast(SILBuilder &B, SILLocation loc,
                                               SILDynamicCastInst inst);

SILValue emitSuccessfulScalarUnconditionalCast(
    SILBuilder &B, ModuleDecl *M, SILLocation loc, SILValue value,
    SILType loweredTargetType,
    CanType formalSourceType, CanType formalTargetType,
    SILInstruction *existingCast = nullptr);

bool emitSuccessfulIndirectUnconditionalCast(
    SILBuilder &B, ModuleDecl *M, SILLocation loc,
    SILValue src, CanType sourceType,
    SILValue dest, CanType targetType,
    SILInstruction *existingCast = nullptr);

bool emitSuccessfulIndirectUnconditionalCast(SILBuilder &B, SILLocation loc,
                                             SILDynamicCastInst dynamicCast);

/// Can the given cast be performed by the scalar checked-cast
/// instructions, or does we need to use the indirect instructions?
bool canUseScalarCheckedCastInstructions(SILModule &M,
                                         CanType sourceType,CanType targetType);

/// Carry out the operations required for an indirect conditional cast
/// using a scalar cast operation.
void emitIndirectConditionalCastWithScalar(
    SILBuilder &B, ModuleDecl *M, SILLocation loc,
    CastConsumptionKind consumption, SILValue src, CanType sourceType,
    SILValue dest, CanType targetType, SILBasicBlock *trueBB,
    SILBasicBlock *falseBB, ProfileCounter TrueCount = ProfileCounter(),
    ProfileCounter FalseCount = ProfileCounter());

/// Does the type conform to the _ObjectiveCBridgeable protocol.
bool isObjectiveCBridgeable(ModuleDecl *M, CanType Ty);

/// Get the bridged NS class of a CF class if it exists. Returns
/// an empty CanType if such class does not exist.
CanType getNSBridgedClassOfCFClass(ModuleDecl *M, CanType type);

/// Does the type conform to Error.
bool isError(ModuleDecl *M, CanType Ty);

struct SILDynamicCastKind {
  enum innerty : std::underlying_type<SILInstructionKind>::type {
#define DYNAMICCAST_INST(ID, PARENT) ID = unsigned(SILInstructionKind::ID),
#include "swift/SIL/SILNodes.def"
  } value;

  explicit SILDynamicCastKind(SILInstructionKind kind) {
    auto newValue = SILDynamicCastKind::fromNodeKindHelper(kind);
    assert(newValue && "Non cast passed into SILDynamicCastKind");
    value = newValue.getValue();
  }

  SILDynamicCastKind(innerty value) : value(value) {}
  operator innerty() const { return value; }

  static Optional<SILDynamicCastKind> fromNodeKind(SILInstructionKind kind) {
    if (auto innerTyOpt = SILDynamicCastKind::fromNodeKindHelper(kind))
      return SILDynamicCastKind(*innerTyOpt);
    return None;
  }

private:
  static Optional<innerty> fromNodeKindHelper(SILInstructionKind kind) {
    switch (kind) {
#define DYNAMICCAST_INST(ID, PARENT)                                           \
  case SILInstructionKind::ID:                                                 \
    return SILDynamicCastKind::ID;
#include "swift/SIL/SILNodes.def"
    default:
      return None;
    }
  }
};

struct SILDynamicCastInst {
  SILInstruction *inst;

public:
  SILDynamicCastInst() : inst(nullptr) {}

  explicit SILDynamicCastInst(SILInstruction *inst) : inst(inst) {
    assert(classof(inst) && "not a dynamic cast?!");
  }

  static bool classof(const SILInstruction *inst) {
    return bool(SILDynamicCastKind::fromNodeKind(inst->getKind()));
  }

#define DYNAMICCAST_INST(ID, PARENT)                                           \
  SILDynamicCastInst(ID *i) : inst(i) {}
#include "swift/SIL/SILNodes.def"

  static SILDynamicCastInst getAs(SILNode *node) {
    auto *i = dyn_cast<SILInstruction>(node);
    if (!i)
      return SILDynamicCastInst();
    auto kind = SILDynamicCastKind::fromNodeKind(i->getKind());
    if (!kind)
      return SILDynamicCastInst();
    switch (kind.getValue()) {
#define DYNAMICCAST_INST(ID, PARENT)                                           \
  case SILDynamicCastKind::ID:                                                 \
    return SILDynamicCastInst(cast<ID>(node));
#include "swift/SIL/SILNodes.def"
    }
  }

  SILDynamicCastKind getKind() const {
    return SILDynamicCastKind(inst->getKind());
  }

  explicit operator bool() const { return inst != nullptr; }

  SILInstruction *getInstruction() const { return inst; }

  CastConsumptionKind getBridgedConsumptionKind() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      return cast<CheckedCastAddrBranchInst>(inst)->getConsumptionKind();
    // TODO: Bridged casts cannot be expressed by checked_cast_br or
    // checked_cast_value_br yet. Should we ever support it, please
    // review this code.
    case SILDynamicCastKind::CheckedCastBranchInst:
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      return CastConsumptionKind::CopyOnSuccess;
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
      return CastConsumptionKind::TakeAlways;
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return CastConsumptionKind::CopyOnSuccess;
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  CastConsumptionKind getConsumptionKind() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
    case SILDynamicCastKind::CheckedCastBranchInst:
    case SILDynamicCastKind::CheckedCastValueBranchInst:
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  SILBasicBlock *getSuccessBlock() {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      return cast<CheckedCastAddrBranchInst>(inst)->getSuccessBB();
    case SILDynamicCastKind::CheckedCastBranchInst:
      return cast<CheckedCastBranchInst>(inst)->getSuccessBB();
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      return cast<CheckedCastValueBranchInst>(inst)->getSuccessBB();
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return nullptr;
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  Optional<ProfileCounter> getSuccessBlockCount() {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      llvm_unreachable("unsupported");
    case SILDynamicCastKind::CheckedCastBranchInst:
      return cast<CheckedCastBranchInst>(inst)->getTrueBBCount();
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      llvm_unreachable("unsupported");
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return None;
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  const SILBasicBlock *getSuccessBlock() const {
    return const_cast<SILDynamicCastInst &>(*this).getSuccessBlock();
  }

  SILBasicBlock *getFailureBlock() {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      return cast<CheckedCastAddrBranchInst>(inst)->getFailureBB();
    case SILDynamicCastKind::CheckedCastBranchInst:
      return cast<CheckedCastBranchInst>(inst)->getFailureBB();
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      return cast<CheckedCastValueBranchInst>(inst)->getFailureBB();
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return nullptr;
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  Optional<ProfileCounter> getFailureBlockCount() {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      llvm_unreachable("unsupported");
    case SILDynamicCastKind::CheckedCastBranchInst:
      return cast<CheckedCastBranchInst>(inst)->getFalseBBCount();
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      llvm_unreachable("unsupported");
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return None;
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  const SILBasicBlock *getFailureBlock() const {
    return const_cast<SILDynamicCastInst &>(*this).getFailureBlock();
  }

  SILValue getSource() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      return cast<CheckedCastAddrBranchInst>(inst)->getSrc();
    case SILDynamicCastKind::CheckedCastBranchInst:
      return cast<CheckedCastBranchInst>(inst)->getOperand();
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      return cast<CheckedCastValueBranchInst>(inst)->getOperand();
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
      return cast<UnconditionalCheckedCastAddrInst>(inst)->getSrc();
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return cast<UnconditionalCheckedCastInst>(inst)->getOperand();
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  // Returns the success value.
  SILValue getDest() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      return cast<CheckedCastAddrBranchInst>(inst)->getDest();
    case SILDynamicCastKind::CheckedCastBranchInst:
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      // TODO: Shouldn't this return getSuccessBlock()->getArgument(0)?
      return SILValue();
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
      return cast<UnconditionalCheckedCastAddrInst>(inst)->getDest();
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      // TODO: Why isn't this:
      //
      // return cast<UnconditionalCheckedCastInst>(inst);
      return SILValue();
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unimplemented");
    }
  }

  CanType getSourceType() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      return cast<CheckedCastAddrBranchInst>(inst)->getSourceType();
    case SILDynamicCastKind::CheckedCastBranchInst:
      return cast<CheckedCastBranchInst>(inst)->getSourceType();
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      return cast<CheckedCastValueBranchInst>(inst)->getSourceType();
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
      return cast<UnconditionalCheckedCastAddrInst>(inst)->getSourceType();
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return cast<UnconditionalCheckedCastInst>(inst)->getSourceType();
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  SILType getLoweredSourceType() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
    case SILDynamicCastKind::CheckedCastBranchInst:
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      llvm_unreachable("unsupported");
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst: {
      auto *uccai = cast<UnconditionalCheckedCastAddrInst>(inst);
      return uccai->getSrc()->getType();
    }
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return cast<UnconditionalCheckedCastInst>(inst)->getOperand()->getType();
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  CanType getTargetType() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      return cast<CheckedCastAddrBranchInst>(inst)->getTargetType();
    case SILDynamicCastKind::CheckedCastBranchInst:
      return cast<CheckedCastBranchInst>(inst)->getTargetType();
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      return cast<CheckedCastValueBranchInst>(inst)->getTargetType();
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
      return cast<UnconditionalCheckedCastAddrInst>(inst)->getTargetType();
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return cast<UnconditionalCheckedCastInst>(inst)->getTargetType();
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unimplemented");
    }
  }

  SILType getLoweredTargetType() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
      llvm_unreachable("unsupported");
    case SILDynamicCastKind::CheckedCastBranchInst:
      return cast<CheckedCastBranchInst>(inst)->getCastType();
    case SILDynamicCastKind::CheckedCastValueBranchInst:
      return cast<CheckedCastValueBranchInst>(inst)->getCastType();
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst: {
      auto *uccai = dyn_cast<UnconditionalCheckedCastAddrInst>(inst);
      return uccai->getDest()->getType();
    }
    case SILDynamicCastKind::UnconditionalCheckedCastInst: {
      return cast<UnconditionalCheckedCastInst>(inst)->getType();
    }
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  bool isSourceTypeExact() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastValueBranchInst:
    case SILDynamicCastKind::CheckedCastBranchInst:
    case SILDynamicCastKind::CheckedCastAddrBranchInst:
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return isa<MetatypeInst>(getSource());
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  SILLocation getLocation() const { return inst->getLoc(); }

  SILModule &getModule() const { return inst->getModule(); }
  SILFunction *getFunction() const { return inst->getFunction(); }

  DynamicCastFeasibility classifyFeasibility(bool allowWholeModule) const {
    return swift::classifyDynamicCast(
        getModule().getSwiftModule(), getSourceType(), getTargetType(),
        isSourceTypeExact(), allowWholeModule && getModule().isWholeModule());
  }

  bool isBridgingCast() const {
    // Bridging casts cannot be further simplified.
    auto TargetIsBridgeable = getTargetType()->isBridgeableObjectType();
    auto SourceIsBridgeable = getSourceType()->isBridgeableObjectType();
    return TargetIsBridgeable != SourceIsBridgeable;
  }

  /// If getSourceType() is a Swift type that can bridge to an ObjC type, return
  /// the ObjC type it bridges to. If the source type is an objc type, an empty
  /// CanType() is returned.
  CanType getBridgedSourceType() const {
    SILModule &mod = getModule();
    Type t = mod.getASTContext().getBridgedToObjC(mod.getSwiftModule(),
                                                  getSourceType());
    if (!t)
      return CanType();
    return t->getCanonicalType();
  }

  /// If getTargetType() is a Swift type that can bridge to an ObjC type, return
  /// the ObjC type it bridges to. If the target type is an objc type, an empty
  /// CanType() is returned.
  CanType getBridgedTargetType() const {
    SILModule &mod = getModule();
    Type t = mod.getASTContext().getBridgedToObjC(mod.getSwiftModule(),
                                                  getTargetType());
    if (!t)
      return CanType();
    return t->getCanonicalType();
  }

  Optional<SILType> getLoweredBridgedTargetObjectType() const {
    CanType t = getBridgedTargetType();
    if (!t)
      return None;
    return SILType::getPrimitiveObjectType(t);
  }

  bool isConditional() const {
    switch (getKind()) {
    case SILDynamicCastKind::CheckedCastAddrBranchInst: {
      auto f = classifyFeasibility(true /*allow wmo*/);
      return f == DynamicCastFeasibility::MaySucceed;
    }
    case SILDynamicCastKind::CheckedCastBranchInst: {
      auto f = classifyFeasibility(false /*allow wmo*/);
      return f == DynamicCastFeasibility::MaySucceed;
    }
    case SILDynamicCastKind::CheckedCastValueBranchInst: {
      auto f = classifyFeasibility(false /*allow wmo opts*/);
      return f == DynamicCastFeasibility::MaySucceed;
    }
    case SILDynamicCastKind::UnconditionalCheckedCastAddrInst:
    case SILDynamicCastKind::UnconditionalCheckedCastInst:
      return false;
    case SILDynamicCastKind::UnconditionalCheckedCastValueInst:
      llvm_unreachable("unsupported");
    }
  }

  bool canUseScalarCheckedCastInstructions() const {
    return swift::canUseScalarCheckedCastInstructions(
        getModule(), getSourceType(), getTargetType());
  }
};

} // end namespace swift

#endif

