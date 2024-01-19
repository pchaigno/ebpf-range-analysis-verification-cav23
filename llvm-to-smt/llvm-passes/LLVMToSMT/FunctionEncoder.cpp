#include "FunctionEncoder.hpp"
#include <llvm/IR/Instructions.h>
#include <stdexcept>

std::string FunctionEncoder::toString() {
  z3::expr f = z3::mk_and(this->functionEncodingZ3ExprVec);
  z3::solver solv = z3::solver(ctx);
  solv.add(f);

  Z3_set_ast_print_mode(ctx, Z3_PRINT_SMTLIB_FULL);
  std::string os;
  llvm::raw_string_ostream llvmos(os);
  std::ostringstream oss;
  Json::StyledWriter styledWriter;

  llvmos << "---------\n";
  llvmos << "Function: " << this->currentFunction->getName() << "\n";
  llvmos << "---------\n";

  llvmos << "---------\n";
  llvmos << "Inputs: \n";
  std::string inputJsonDictStr = styledWriter.write(*(this->inputJsonDict));
  llvmos << inputJsonDictStr << "\n";
  llvmos << "---------\n";

  llvmos << "\n---------\n";
  llvmos << "Outputs: \n";
  std::string outputJsonDictStr = styledWriter.write(*(this->outputJsonDict));
  llvmos << outputJsonDictStr << "\n";
  llvmos << "\n---------\n";

  llvmos << "Solver: \n";
  llvmos << solv.to_smt2();
  llvmos << "\n---------\n";

  return llvmos.str();
}

void FunctionEncoder::printEdgeAssertionsMap() {
  for (auto &pair : EdgeAssertionsMap) {
    outs() << "<" << std::get<0>(pair).first->getName() << ", "
           << std::get<0>(pair).second->getName() << "> :";
    outs() << std::get<1>(pair).to_string().c_str() << "\n";
  }
}

void FunctionEncoder::printPathConditionsMap() {
  for (auto KeyValue : pathConditionsMap) {
    BasicBlock *BB = KeyValue.first;
    z3::expr Z3Expr = KeyValue.second;
    outs() << BB->getName() << "\n";
    outs() << "  " << Z3Expr.to_string().c_str() << "\n";
  }
}

void FunctionEncoder::printBBAssertionsMap(BasicBlock *BB) {
  outs() << "[printBBAssertionsMap] "
         << "\n";
  if (BB) {
    for (auto Z3Expr : BBAssertionsMap.at(BB)) {
      outs() << "  " << Z3Expr.to_string().c_str() << "\n";
    }
    outs() << "\n";
    return;
  }
  for (auto KeyValue : BBAssertionsMap) {
    BasicBlock *BB = KeyValue.first;
    z3::expr_vector Z3ExprVec = KeyValue.second;
    outs() << "BasicBlock: " << BB->getName() << ", Size:" << Z3ExprVec.size()
           << "\n";
    for (auto Z3Expr : Z3ExprVec) {
      outs() << "  " << Z3Expr.to_string().c_str() << "\n";
    }
  }
  outs() << "\n";
}

std::string FunctionEncoder::ValueBVTreeMapToString(ValueBVTreeMap vt) {
  std::string os;
  llvm::raw_string_ostream llvmos(os);
  for (auto kv : vt) {
    Value *v = kv.first;
    BVTree *bvt = kv.second;
    llvmos << "- " << v->getName() << ": " << bvt->toString() << "\n";
  }
  return llvmos.str();
}

void FunctionEncoder::printValueBVTreeMap(ValueBVTreeMap vt) {
  outs() << ValueBVTreeMapToString(vt) << "\n";
  outs().flush();
}

void FunctionEncoder::printMemoryAccessValueBVTreeMap() {
  std::string os;
  llvm::raw_string_ostream llvmos(os);
  for (std::pair<MemoryAccess *, ValueBVTreeMap> kv :
       MemoryAccessValueBVTreeMap) {
    MemoryAccess *ma = kv.first;
    ValueBVTreeMap vt = kv.second;
    llvmos << *ma << " : {\n" << ValueBVTreeMapToString(vt) << "}\n";
  }
  outs() << os << "\n";
}

std::string FunctionEncoder::stdVectorIntToString(std::vector<int> &vec) {
  std::stringstream strStream;
  std::copy(vec.begin(), vec.end(), std::ostream_iterator<int>(strStream, " "));
  return "[ " + strStream.str() + "]";
}

std::string
FunctionEncoder::GEPMapSingleElementToString(Value *v0, Value *v1,
                                             std::vector<int> *gepIndices) {

  std::string os;
  llvm::raw_string_ostream llvmos(os);
  llvmos << v0->getName() << ", " << v1->getName() << ", [";
  for (std::vector<int>::size_type i = 0; i < gepIndices->size(); i++) {
    llvmos << gepIndices->at(i);
    if (i < gepIndices->size() - 1) {
      llvmos << ", ";
    }
  }
  llvmos << "]";
  return llvmos.str();
}

void FunctionEncoder::printGEPMap() {
  for (auto kv : GEPMap) {
    Value *v0 = kv.first;
    Value *v1 = kv.second.first;
    std::vector<int> *gepIndices = kv.second.second;
    outs() << GEPMapSingleElementToString(v0, v1, gepIndices);
    outs() << "\n";
  }
}

void FunctionEncoder::printSelectMap() {
  for (auto kv : SelectMap) {
    Value *v0 = kv.first;
    Value *v1 = kv.second.first;
    Value *v2 = kv.second.second;
    outs() << v0->getName() << "," << v1->getName() << "," << v2->getName();
    outs() << "\n";
  }
}

void FunctionEncoder::printPhiMap() {
  std::string os;
  llvm::raw_string_ostream llvmos(os);
  for (auto kv : PhiMap) {
    Value *phiValue = kv.first;
    llvmos << phiValue->getName();
    llvmos << " : [";
    std::vector<ValueBBPair> phiMapEntries = kv.second;
    for (const auto &valueBBPair : phiMapEntries) {
      llvmos << "<";
      llvmos << valueBBPair.first->getName();
      llvmos << ", ";
      llvmos << valueBBPair.second->getName();
      llvmos << ">";
      llvmos << ", ";
    }
    llvmos << "]\n";
  }
  outs() << os;
}

void FunctionEncoder::printPhiResolutionMap() {
  for (auto &bbPair : PhiResolutionMap) {
    outs() << "<" << std::get<0>(bbPair).first->getName() << ", "
           << std::get<0>(bbPair).second->getName() << ">: ";
    z3::expr Z3Expr = std::get<1>(bbPair);
    outs() << Z3Expr.to_string().c_str() << "\n";
  }
}

void FunctionEncoder::printMemoryPhiResolutionMap() {
  for (auto &bbPair : MemoryPhiResolutionMap) {
    outs() << "<" << std::get<0>(bbPair).first->getName() << ", "
           << std::get<0>(bbPair).second->getName() << "> :";
    z3::expr_vector Z3ExprVec = std::get<1>(bbPair);
    for (auto Z3Expr : Z3ExprVec) {
      outs() << "\n  " << Z3Expr.to_string().c_str();
    }
    outs() << "\n";
  }
}

std::string Z3ExprVecToString(z3::expr_vector &v) {
  std::ostringstream oss;
  oss << "[";
  for (z3::expr e : v) {
    oss << e << "\n";
  }
  oss << "]";
  return oss.str();
}

// -----------------------------------------------------------------------------

void FunctionEncoder::handleCastInst(CastInst &i) {
  outs() << "[handleCastInst]\n";

  if (isa<BitCastInst>(&i)) {
    throw std::runtime_error("[handleCallInst]"
                             "bitcast instruction not supported\n");
  }

  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);
  Value *op0Val = i.getOperand(0);
  Value *opResVal = dyn_cast<Value>(&i);

  z3::expr op0BV = BitVecHelper::getBitVecSingValType(op0Val);
  z3::expr opResBV = BitVecHelper::getBitVecSingValType(opResVal);
  unsigned op0BVWidth = op0BV.get_sort().bv_size();
  unsigned opResBVWidth = opResBV.get_sort().bv_size();
  z3::expr resultExpr(ctx);

  if (isa<TruncInst>(&i)) {
    outs() << "[handleCastInst] "
           << "TRUNC instruction\n";
    resultExpr = (opResBV == op0BV.extract(opResBVWidth - 1, 0));
  } else if (isa<ZExtInst>(&i)) {
    outs() << "[handleCastInst]"
           << "ZEXT instruction\n";
    resultExpr = (opResBV == z3::zext(op0BV, opResBVWidth - op0BVWidth));
  } else if (isa<SExtInst>(&i)) {
    outs() << "[handleCastInst]"
           << "SEXT instruction\n";
    resultExpr = (opResBV == z3::sext(op0BV, opResBVWidth - op0BVWidth));
  } else {
    errs() << "[handleCastInst]"
           << "Unknown CAST instruction: " << i << "\n";
  }

  outs() << resultExpr.to_string().c_str() << "\n";
  BBAsstVecIter->second.push_back(resultExpr);
}

void FunctionEncoder::handleBinaryOperatorInst(BinaryOperator &i) {
  outs() << "[handleBinaryOperatorInst]\n";

  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);
  Value *op0Val = i.getOperand(0);
  Value *op1Val = i.getOperand(1);
  Value *opResVal = dyn_cast<Value>(&i);
  outs() << "[handleBinaryOperatorInst] "
         << "op0Val: " << *op0Val << "\n";
  outs() << "[handleBinaryOperatorInst] "
         << "op1Val: " << *op1Val << "\n";
  outs() << "[handleBinaryOperatorInst] "
         << "opResVal: " << *opResVal << "\n";

  z3::expr op0BV = BitVecHelper::getBitVecSingValType(op0Val);
  z3::expr op1BV = BitVecHelper::getBitVecSingValType(op1Val);
  z3::expr opResBV = BitVecHelper::getBitVecSingValType(opResVal);
  z3::expr resultExpr(ctx);

  switch (i.getOpcode()) {

  case Instruction::And: {
    outs() << "[handleBinaryOperatorInst] "
           << "AND instruction"
           << "\n";
    resultExpr = (opResBV == (op0BV & op1BV));
  } break;
  case Instruction::Or: {
    outs() << "[handleBinaryOperatorInst] "
           << "OR instruction"
           << "\n";
    resultExpr = (opResBV == (op0BV | op1BV));
  } break;
  case Instruction::Xor: {
    outs() << "[handleBinaryOperatorInst] "
           << "XOR instruction"
           << "\n";
    resultExpr = (opResBV == (op0BV ^ op1BV));
  } break;
  case Instruction::Add: {
    outs() << "[handleBinaryOperatorInst] "
           << "ADD instruction"
           << "\n";
    resultExpr = (opResBV == (op0BV + op1BV));
  } break;
  case Instruction::Sub: {
    outs() << "[handleBinaryOperatorInst] "
           << "SUB instruction"
           << "\n";
    resultExpr = (opResBV == (op0BV - op1BV));
  } break;
  case Instruction::Mul: {
    outs() << "[handleBinaryOperatorInst] "
           << "MUL instruction"
           << "\n";
    resultExpr = (opResBV == (op0BV * op1BV));
  } break;
  case Instruction::UDiv: {
    outs() << "[handleBinaryOperatorInst] "
           << "UDIV instruction"
           << "\n";
    resultExpr = (opResBV == (z3::udiv(op0BV, op1BV)));
  } break;
  case Instruction::Shl: {
    outs() << "[handleBinaryOperatorInst] "
           << "SHL instruction"
           << "\n";
    resultExpr = opResBV == z3::shl(op0BV, op1BV);
  } break;
  case Instruction::LShr: {
    outs() << "[handleBinaryOperatorInst] "
           << "LSHR instruction"
           << "\n";
    resultExpr = opResBV == z3::lshr(op0BV, op1BV);
  } break;
  case Instruction::AShr: {
    outs() << "[handleBinaryOperatorInst] "
           << "ASHR instruction"
           << "\n";
    resultExpr = opResBV == z3::ashr(op0BV, op1BV);
  } break;
  default:
    break;
  }
  outs() << resultExpr.to_string().c_str() << "\n";
  BBAsstVecIter->second.push_back(resultExpr);
}

/* If function has pointer arguments, construct new outputBitvectorTree(s)
 * corresponding to each pointer argument. These BVTrees will contain bitvectors
 * which are outputs to the function. Assert equivalences in the BBAssertionMap
 * to the bitvectors contained in the outputBitvectorTree(s), by looking at the
 * return instructions memoryDef.
 */
void FunctionEncoder::handleReturnInstPointerArgs(ReturnInst &i) {
  outs() << "[handleReturnInstPointerArgs] "
         << "\n";
  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);
  outs() << "[handleReturnInstPointerArgs] "
         << "currentBB: " << currentBB->getName() << "\n";
  /* Return instruction will correspond to the mostRecentMemoryDef as we support
   * only single return instruction. */
  outs() << "[handleReturnInstPointerArgs] "
         << "mostRecentMemoryDef: " << *mostRecentMemoryDef << "\n";
  ValueBVTreeMap returnInstValueBVTreeMap =
      MemoryAccessValueBVTreeMap.at(mostRecentMemoryDef);

  /* For every pointer argument in the function, construct output equivalence z3
   * expressions. Note that we could have just updated outputValueBVTreeMap with
   * the BVTree for every arg from the mostRecentMemoryDef. But this doesn't
   * work if the function we are looking at has only one BasicBlock withs
   * statements that only update memory, and no updates are done to the
   * BBAssertionMap. Calling getEquivVector() constructs the bitvector
   * equivalences, and asserts them in the formula.
   */
  for (auto argIterator = currentFunction->arg_begin();
       argIterator != currentFunction->arg_end(); argIterator++) {
    Value *argVal = dyn_cast<Value>(argIterator);
    outs() << "[handleReturnInstPointerArgs] "
           << "argVal: " << *argVal << "\n";
    Type *argType = argIterator->getType();
    if (argType->isPointerTy()) {
      z3::expr_vector outputEqExprs(ctx);
      BVTree *outputBVTreeForArg =
          setupBVTreeForArg(argVal, argVal->getName().str());
      BVTree *originalBVTreeForArg = returnInstValueBVTreeMap.at(argVal);
      outs() << "[handleReturnInstPointerArgs] "
             << "originalBVTreeForArg: " << originalBVTreeForArg->toString()
             << "\n";
      BVTree::getEquivVector(originalBVTreeForArg, outputBVTreeForArg,
                             outputEqExprs);
      outs() << "[handleReturnInstPointerArgs] "
             << "outputEqExprs:\n";
      for (auto expr : outputEqExprs) {
        outs() << expr.to_string() << "\n";
        BBAsstVecIter->second.push_back(expr);
      }
      this->outputValueBVTreeMap->insert({argVal, outputBVTreeForArg});
    }
  }

  outs() << "[handleReturnInstPointerArgs] "
         << "outputValueBVTreeMap updated: \n";
  printValueBVTreeMap(*this->outputValueBVTreeMap);
  outs() << "[handleReturnInstPointerArgs] "
         << "currentBB AssertionsMap updated: \n";
  printBBAssertionsMap(currentBB);
}

bool FunctionEncoder::functionHasPointerArguments(Function &F) {
  outs() << "[functionHasPointerArguments]\n";
  for (auto argIterator = F.arg_begin(); argIterator != F.arg_end();
       argIterator++) {
    Type *t = argIterator->getType();
    if (t->isPointerTy()) {
      return true;
    }
  }
  return false;
}

void FunctionEncoder::handleReturnInst(ReturnInst &i) {
  outs() << "[handleReturnInst]\n";
  auto l = i.getReturnValue();
  if (functionReturnsVoid) {
    outs() << "[handleReturnInst]"
           << "return value: void\n";
  } else {
    outs() << "[handleReturnInst]"
           << "return value: " << *l << "\n";
  }

  /* We handle only cases where the last BasicBlock has a return instruction.
   * --mergereturn takes care of this. TODO confirm this. */
  if (i.getNumSuccessors() != 0) {
    throw std::logic_error("Function has multiple return statements \n");
  }

  bool allArgsReadOnlyAndNoCapture = true;
  for (auto argIterator = currentFunction->arg_begin();
       argIterator != currentFunction->arg_end(); argIterator++) {
    allArgsReadOnlyAndNoCapture &=
        argIterator->hasNoCaptureAttr() && argIterator->onlyReadsMemory();
  }

  outs() << "[handleReturnInst]"
         << "allArgsReadOnlyAndNoCapture: " << allArgsReadOnlyAndNoCapture
         << "\n";
  outs() << "[handleReturnInst]"
         << "mostRecentMemoryDef: " << *mostRecentMemoryDef << "\n";

  /* void foo(some_type* a) */
  if (functionReturnsVoid) {
    outs() << "[handleReturnInst]"
           << "Function returns void\n";
    if (functionHasPointerArguments(*currentFunction)) {
      FunctionEncoder::handleReturnInstPointerArgs(i);
    }
    return;
  } else {
    throw std::runtime_error("Function has non-void return type \n");
  }
}

/* TODO only handling unsigned predicates now, add signed */
void FunctionEncoder::handleICmpInst(ICmpInst &i) {
  outs() << "[handleICmpInst]\n";
  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);

  Value *lhs = i.getOperand(0);
  Value *rhs = i.getOperand(1);
  Value *res = dyn_cast<Value>(&i);

  CmpInst::Predicate pred = i.getPredicate();

  auto z3_exp_lhs = BitVecHelper::getBitVecSingValType(lhs);
  auto z3_exp_rhs = BitVecHelper::getBitVecSingValType(rhs);
  auto z3_exp_res = BitVecHelper::getBitVecSingValType(res);

  z3::expr z3_exp_pred(ctx);

  if (pred == CmpInst::ICMP_UGT) {
    z3_exp_pred = z3::ugt(z3_exp_lhs, z3_exp_rhs);
  } else if (pred == CmpInst::ICMP_UGE) {
    z3_exp_pred = z3::uge(z3_exp_lhs, z3_exp_rhs);
  } else if (pred == CmpInst::ICMP_ULT) {
    z3_exp_pred = z3::ult(z3_exp_lhs, z3_exp_rhs);
  } else if (pred == CmpInst::ICMP_ULE) {
    z3_exp_pred = z3::ule(z3_exp_lhs, z3_exp_rhs);
  } else if (pred == CmpInst::ICMP_SGT) {
    z3_exp_pred = (z3_exp_lhs > z3_exp_rhs);
  } else if (pred == CmpInst::ICMP_SGE) {
    z3_exp_pred = (z3_exp_lhs >= z3_exp_rhs);
  } else if (pred == CmpInst::ICMP_SLT) {
    z3_exp_pred = z3::slt(z3_exp_lhs, z3_exp_rhs);
  } else if (pred == CmpInst::ICMP_SLE) {
    z3_exp_pred = z3::sle(z3_exp_lhs, z3_exp_rhs);
  } else if (pred == CmpInst::ICMP_NE) {
    z3_exp_pred = z3_exp_lhs != z3_exp_rhs;
  } else if (pred == CmpInst::ICMP_EQ) {
    z3_exp_pred = z3_exp_lhs == z3_exp_rhs;
  } else {
    throw std::invalid_argument(
        "[handleICmpInst] Unsupported icmp predicate \n");
  }
  z3::expr ICMPExpr = z3::ite(z3_exp_pred, z3_exp_res == 1, z3_exp_res == 0);
  outs() << "[handleICmpInst] " << ICMPExpr.to_string().c_str() << "\n";
  BBAsstVecIter->second.push_back(ICMPExpr);
  printBBAssertionsMap();
}

void FunctionEncoder::handleSelectInst(SelectInst &i) {
  outs() << "[handleSelectInst]\n";

  outs() << "[handleSelectInst]"
         << "mostRecentMemoryDef: " << *mostRecentMemoryDef << "\n";

  Value *selectInstValue = dyn_cast<Value>(&i);
  Value *selectOp1 = i.getOperand(0);
  Value *selectOp2 = i.getOperand(1);
  Value *selectOp3 = i.getOperand(2);

  ValueBVTreeMap selectInstValueBVTreeMap =
      MemoryAccessValueBVTreeMap.at(mostRecentMemoryDef);
  printValueBVTreeMap(selectInstValueBVTreeMap);
  ValuePair selectOperandsValuePair = std::make_pair(selectOp2, selectOp3);
  SelectMap.insert({selectInstValue, selectOperandsValuePair});
  outs() << "[handleSelectInst]"
         << "SelectMap:"
         << "\n";
  printSelectMap();

  if (!selectInstValue->getType()->isPointerTy()) {
    outs() << "[handleSelectInst]"
           << "select instruction is not a pointer type:"
           << "\n";
    auto z3ExprSelectOp1 = BitVecHelper::getBitVecSingValType(selectOp1);
    auto z3ExprSelectOp2 = BitVecHelper::getBitVecSingValType(selectOp2);
    auto z3ExprSelectOp3 = BitVecHelper::getBitVecSingValType(selectOp3);
    auto z3ExprRes = BitVecHelper::getBitVecSingValType(selectInstValue);

    auto BBAsstVecIter = BBAssertionsMap.find(currentBB);

    outs() << "[handleSelectInst] z3ExprSelectOp1: "
           << z3ExprSelectOp1.to_string() << "\n";
    outs() << "[handleSelectInst] z3ExprSelectOp2: "
           << z3ExprSelectOp2.to_string() << "\n";
    outs() << "[handleSelectInst] z3ExprSelectOp3: "
           << z3ExprSelectOp3.to_string() << "\n";
    outs() << "[handleSelectInst] z3ExprRes: " << z3ExprRes.to_string() << "\n";
    outs().flush();
    z3::expr selectEncoding =
        (z3::ite((z3ExprSelectOp1 == 1), (z3ExprRes == z3ExprSelectOp2),
                 (z3ExprRes == z3ExprSelectOp3)));
    outs() << "[handleSelectInst]" << selectEncoding.to_string().c_str()
           << "\n";

    BBAsstVecIter->second.push_back(selectEncoding);
  }
}

void FunctionEncoder::handleBranchInst(BranchInst &i) {
  outs() << "[handleBranchInst] "
         << "EdgeAssertionsMap:\n";
  printEdgeAssertionsMap();
  outs() << "[handleBranchInst] "
         << "PathConditionsMap:\n";
  printPathConditionsMap();

  if (i.isConditional()) {
    outs() << "[handleBranchInst] "
           << " conditional branch \n";

    /* Support only 2 successors for a conditional branch */
    assert(i.getNumSuccessors() == 2);

    z3::expr cond = BitVecHelper::getBitVecSingValType(i.getCondition());
    BasicBlock *destBB1 = i.getSuccessor(0);
    BasicBlock *destBB2 = i.getSuccessor(1);

    /* Create new path conditions for each destination BB */
    z3::expr newPathCond1 = (cond == 1);
    z3::expr newPathCond2 = (cond == 0);

    /* If there exists a path condition for currentBB, conjunct it*/
    if (pathConditionsMap.find(currentBB) != pathConditionsMap.end()) {
      newPathCond1 = (newPathCond1 && pathConditionsMap.at(currentBB));
      newPathCond2 = (newPathCond2 && pathConditionsMap.at(currentBB));
    }

    /* First update EdgeAssertionsMap */
    BBPair BBPair1 = std::make_pair(currentBB, destBB1);
    BBPair BBPair2 = std::make_pair(currentBB, destBB2);
    assert(EdgeAssertionsMap.find(BBPair1) == EdgeAssertionsMap.end());
    assert(EdgeAssertionsMap.find(BBPair2) == EdgeAssertionsMap.end());
    EdgeAssertionsMap.insert({BBPair1, newPathCond1});
    EdgeAssertionsMap.insert({BBPair2, newPathCond2});

    outs() << "[handleBranchInst] "
           << "Edge Assertions:\n";
    outs() << "<" << BBPair1.first->getName() << ", "
           << BBPair1.second->getName() << ">"
           << ": " << newPathCond1.to_string().c_str() << "\n";
    outs() << "<" << BBPair2.first->getName() << ", "
           << BBPair2.second->getName() << ">"
           << ": " << newPathCond2.to_string().c_str() << "\n";

    /* Now update PathConditionsMap. If there exists a path condition for the
     * destBB disjunct it and finally, update pathConditionsMap */
    auto PCIter1 = pathConditionsMap.find(destBB1);
    if (PCIter1 != pathConditionsMap.end()) {
      newPathCond1 = (newPathCond1 || pathConditionsMap.at(destBB1));
      PCIter1->second = newPathCond1;
    } else {
      pathConditionsMap.insert({destBB1, newPathCond1});
    }
    auto PCIter2 = pathConditionsMap.find(destBB2);
    if (PCIter2 != pathConditionsMap.end()) {
      newPathCond2 = (newPathCond2 || pathConditionsMap.at(destBB2));
      PCIter2->second = newPathCond2;
    } else {
      pathConditionsMap.insert({destBB2, newPathCond2});
    }

    outs() << "[handleBranchInst] "
           << "Path Conditions:\n";

    outs() << "[handleBranchInst] " << destBB1->getName() << ": "
           << pathConditionsMap.at(destBB1).to_string().c_str() << "\n";
    outs() << "[handleBranchInst] " << destBB2->getName() << ": "
           << pathConditionsMap.at(destBB2).to_string().c_str() << "\n";
    outs() << "[handleBranchInst] "
           << "<" << BBPair1.first->getName() << ", "
           << BBPair1.second->getName()
           << "> :" << EdgeAssertionsMap.at(BBPair1).to_string().c_str()
           << "\n";
    outs() << "[handleBranchInst] "
           << "<" << BBPair2.first->getName() << ", "
           << BBPair2.second->getName()
           << "> :" << EdgeAssertionsMap.at(BBPair2).to_string().c_str()
           << "\n";

  } else {
    outs() << "[handleBranchInst] "
           << "unconditional branch \n";
    BasicBlock *destBB = i.getSuccessor(0);
    outs() << "[handleBranchInst] "
           << "destBB: " << destBB->getName() << "\n";

    z3::expr newPathCond(ctx);
    if (pathConditionsMap.find(currentBB) == pathConditionsMap.end()) {
      newPathCond = ctx.bool_val(true);
    } else {
      newPathCond = pathConditionsMap.at(currentBB);
    }
    outs() << "[handleBranchInst] "
           << "newPathCond: " << newPathCond.to_string().c_str() << "\n";

    /* Update EdgeAssertionsMap */
    BBPair currentBBPair = std::make_pair(currentBB, destBB);
    assert(EdgeAssertionsMap.find(currentBBPair) == EdgeAssertionsMap.end());
    EdgeAssertionsMap.insert({currentBBPair, newPathCond});

    /* Update pathConditionsMap*/
    auto PCIter = pathConditionsMap.find(destBB);
    if (PCIter != pathConditionsMap.end()) {
      newPathCond = (newPathCond || pathConditionsMap.at(destBB));
      PCIter->second = newPathCond;
    } else {
      pathConditionsMap.insert({destBB, newPathCond});
    }
    outs() << "[handleBranchInst] " << destBB->getName() << ": "
           << pathConditionsMap.at(destBB).to_string().c_str() << "\n";
    outs() << "[handleBranchInst] "
           << "<" << currentBBPair.first->getName() << ", "
           << currentBBPair.second->getName()
           << "> :" << EdgeAssertionsMap.at(currentBBPair).to_string().c_str()
           << "\n";
  }
  outs() << "[handleBranchInst] "
         << "EdgeAssertionsMap:\n";
  printEdgeAssertionsMap();
  outs() << "[handleBranchInst] "
         << "PathConditionsMap:\n";
  printPathConditionsMap();
}

void FunctionEncoder::handlePhiNodeSetupBitVecs(PHINode &phiInst) {

  Value *phiInstValue = dyn_cast<Value>(&phiInst);
  outs() << "[handlePhiNodeSetupBitVecs]"
         << "phiInstValue:" << *phiInstValue << "\n";

  std::vector<ValueBBPair> *phiMapEntries = new std::vector<ValueBBPair>;

  for (auto i = 0u; i < phiInst.getNumIncomingValues(); i++) {
    Value *valueI = phiInst.getIncomingValue(i);
    outs() << "[handlePhiNodeSetupBitVecs] "
           << "valueI: " << *valueI << "\n";
    BasicBlock *incomingBlockI = phiInst.getIncomingBlock(i);
    BBPair BBPairI = std::make_pair(incomingBlockI, currentBB);
    std::string BBPairStringI =
        incomingBlockI->getName().str() + "_" + currentBB->getName().str();
    z3::expr phiConditionBoolI = BitVecHelper::getBool(BBPairStringI.c_str());
    outs() << "[handlePhiNodeSetupBitVecs] "
           << "phiConditionBoolI: " << phiConditionBoolI.to_string().c_str()
           << "\n";
    PhiResolutionMap.insert({BBPairI, phiConditionBoolI});
    phiMapEntries->push_back(std::make_pair(valueI, incomingBlockI));
  }

  PhiMap.insert({phiInstValue, *phiMapEntries});
  outs() << "[handlePhiNodeSetupBitVecs] "
         << "printPhiMap: "
         << "\n";
  printPhiMap();

  outs() << "[handlePhiNodeSetupBitVecs] "
         << "PhiResolutionMap: "
         << "\n";
  printPhiResolutionMap();

  if (!phiInstValue->getType()->isPointerTy()) {

    auto phiInstBV = BitVecHelper::getBitVecSingValType(phiInstValue);
    auto BBAsstVecIter = BBAssertionsMap.find(currentBB);

    outs() << "[handlePhiNodeSetupBitVecs] "
           << "phi is a not a pointer value type: \n";

    for (auto i = 0u; i < phiInst.getNumIncomingValues(); i++) {
      Value *valueI = phiInst.getIncomingValue(i);
      BasicBlock *incomingBlockI = phiInst.getIncomingBlock(i);
      BBPair BBPairI = std::make_pair(incomingBlockI, currentBB);
      z3::expr phiConditionBoolI = PhiResolutionMap.at(BBPairI);
      auto valueIBV = BitVecHelper::getBitVecSingValType(valueI);
      outs() << "[handlePhiNodeSetupBitVecs] "
             << "phiConditionBoolI: " << phiConditionBoolI.to_string().c_str()
             << "\n";
      z3::expr phiEncodingI =
          z3::implies(phiConditionBoolI, phiInstBV == valueIBV);
      outs() << "[handlePhiNodeSetupBitVecs] "
             << "phiEncodingI: " << phiEncodingI.to_string().c_str() << "\n";
      BBAsstVecIter->second.push_back(phiEncodingI);
    }

    outs() << "[handlePhiNodeSetupBitVecs] "
           << "BBAssertionsMap: "
           << "\n";
    printBBAssertionsMap(currentBB);
  }

  outs().flush();
}

void FunctionEncoder::handlePhiNodeResolvePathConditions(PHINode &inst) {
  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);
  for (auto i = 0u; i < inst.getNumIncomingValues(); i++) {
    BasicBlock *incomingBlockI = inst.getIncomingBlock(i);
    outs() << "[handlePhiNodeResolvePathConditions] "
           << "incomingBlockI " << incomingBlockI->getName() << "\n";
    BBPair BBPairI = std::make_pair(incomingBlockI, currentBB);
    z3::expr phiConditionI = PhiResolutionMap.at(BBPairI);
    outs() << "[handlePhiNodeResolvePathConditions] "
           << "phiConditionI: " << phiConditionI.to_string().c_str() << "\n";
    z3::expr phiPathConditionsI = EdgeAssertionsMap.at(BBPairI);
    outs() << "[handlePhiNodeResolvePathConditions] "
           << "path conditions for BBPairI from EdgeAssertionsMap: "
           << phiPathConditionsI.to_string().c_str() << "\n";
    z3::expr phiResolveI = (phiConditionI == phiPathConditionsI);
    outs() << "[handlePhiNodeResolvePathConditions] "
           << "phiResolveI: " << phiResolveI.to_string().c_str() << "\n";
    BBAsstVecIter->second.push_back(phiResolveI);
  }
  outs() << "[handlePhiNodeResolvePathConditions] "
         << "BBAssertionsMap: "
         << "\n";
  printBBAssertionsMap(currentBB);
  outs().flush();
}

/*
lend:
  %c = phi i32 [ %a, %lfalse ], [ %b, %ltrue ]

In the first pass,
1. Create a bitvector for the PHI instruction: c_bv
2. Create a boolean z3 constant for each edge coming into the basic block:
   lfalse_lend, ltrue_lend
3. Conditinally set c_bv to either a_bv or c_bv:
    (=> lfalse_lend_bool (= c_bv a_bv))
    (=> ltrue_lend_bool (= c_bv b_bv))

In the second pass, the edge conditions have been figured out. Set their values:
4. (= lfalse_lend_bool <pc for lfalse-to-lend from EdgeAssertionsMap>)
   (= ltrue_lend_bool <pc for ltrue-to-lend from EdgeAssertionsMap>)
*/

void FunctionEncoder::handlePhiNode(PHINode &inst, int passID) {
  outs() << "[handlePhiNode]\n";
  outs() << "passID: " << passID << "\n";

  if (passID == 1) {
    handlePhiNodeSetupBitVecs(inst);
    return;
  }

  if (passID == 3) {
    handlePhiNodeResolvePathConditions(inst);
    return;
  }
}

void populateGEPIndices(GetElementPtrInst &i, std::vector<int> &GEPIndices) {

  outs() << "[populateGEPIndices] "
         << "\n";

  if (i.getNumOperands() == 3) {
    int idx;
    /* assume GEP index is 0 if it is not a constant */
    if (BitVecHelper::isValueConstantInt(i.getOperand(2))) {
      idx = BitVecHelper::getConstantIntValue(i.getOperand(2));
    } else {
      idx = 0;
    }
    GEPIndices.push_back(idx);
    outs() << "[populateGEPIndices] "
           << "idx: " << idx << "\n";
  } else if (i.getNumOperands() == 4) {
    int idx0, idx1;
    /* assume GEP index is 0 if it is not a constant */
    if (BitVecHelper::isValueConstantInt(i.getOperand(2))) {
      idx0 = BitVecHelper::getConstantIntValue(i.getOperand(2));
    } else {
      idx0 = 0;
    }
    if (BitVecHelper::isValueConstantInt(i.getOperand(3))) {
      idx1 = BitVecHelper::getConstantIntValue(i.getOperand(3));
    } else {
      idx1 = 0;
    }
    GEPIndices.push_back(idx0);
    GEPIndices.push_back(idx1);
    outs() << "[populateGEPIndices] "
           << "idx0: " << idx0 << ", idx1: " << idx1 << "\n";
  } else {
    throw std::invalid_argument("Unsupported number of GEP indices\n");
  }
}

/*
GEP instructions are only used to update the GEPMap, which stores the base type
pointer of the GEP, and offset operands. Every GEP is resolved to be associated
with an offset into a pointer that is a function argument (like a
struct.bpf_reg_state* dst_reg). If the GEP pointer is not a function argument,
then it better be derived from a function argument (eg. struct.tnum* var_off
derived from dst_reg).

%i = getelementptr inbounds %struct.bpf_reg_state, %struct.bpf_reg_state*
%dst_reg, i64 0, i32 5, i32 0

Here, we store {%i: [%dst_reg, 5, 0]} in the GEPMap.

%j = getelementptr inbounds %struct.tnum, %struct.tnum* %dst_reg, i64 0, i32 5
%k = getelementptr inbounds %struct.tnum, %struct.tnum* %j, i64 0, i32 1

For the above two instructions we store, {%j: [%dst_reg, 5]}, and {%k:
[%dst_reg, 5, 1]} in the GEPMap respectively. Essentially, all GEP's have been
resolved to offset into dst_reg.

*/
void FunctionEncoder::handleGEPInst(GetElementPtrInst &GEPInst) {
  outs() << "[handleGEPInst] "
         << "\n";
  Value *GEPInstVal = dyn_cast<Value>(&GEPInst);
  outs() << "[handleGEPInst] "
         << "GEPInstVal:" << *GEPInstVal << "\n";
  Value *GEPInstArgVal = GEPInst.getOperand(0);
  outs() << "[handleGEPInst] "
         << "GEPInstArgVal: " << *GEPInstArgVal << "\n";
  outs() << "[handleGEPInst] "
         << "getNumOperands:" << GEPInst.getNumOperands() << "\n";
  outs().flush();

  /* Assert that the offset operand to GEP, offsets by 0 elements into the base
   * type, i.e offsets the base type directly */
  auto offsetOperandValueInt =
      BitVecHelper::getConstantIntValue(GEPInst.getOperand(1));
  if (offsetOperandValueInt != 0) {
    outs() << "[handleGEPInst] "
           << "offsetOperandValueInt:" << offsetOperandValueInt << "\n";
    throw std::runtime_error("[handleGEPInst]"
                             "Offset operand to GEP instruction does "
                             "not offset 0 elements into the base type\n");
  }

  std::vector<int> *GEPIndices = new std::vector<int>;
  ValueIndicesPair valueIndicesPairGEP;

  /* If the GEP is looking into a pointer which isn't a function argument (e.g.
   * tnum), then it should be looking at a pointer which has been derived
   * from a function argument (e.g. dst_reg).
   */

  bool GEPOffsetsIntoFunctionArg =
      FunctionArgs.find(GEPInstArgVal) != FunctionArgs.end();

  bool GEPOffsetsIntoArgDerivedFromSelectOrPhi =
      (PhiMap.find(GEPInstArgVal) != PhiMap.end() ||
       SelectMap.find(GEPInstArgVal) != SelectMap.end());

  bool GEPOffsetsIntoArgDerivedFromAnotherGEP =
      GEPMap.find(GEPInstArgVal) != GEPMap.end();

  if (GEPOffsetsIntoFunctionArg || GEPOffsetsIntoArgDerivedFromSelectOrPhi) {
    /*
    GEP offsets into an argument derived from another GEP
    --> %src_u64_min = getelementptr %reg_st, %reg_st* %src_reg, i64 0, i32 4
    Populate the GEPMap like so:
    GEPMap: src_u64_min: <src_reg, [4]>
    */
    valueIndicesPairGEP = std::make_pair(GEPInstArgVal, GEPIndices);
    GEPMap.insert({GEPInstVal, valueIndicesPairGEP});
    populateGEPIndices(GEPInst, *GEPIndices);

  } else if (GEPOffsetsIntoArgDerivedFromAnotherGEP) {

    /*
    GEP offsets into an argument derived from another GEP:
    %src_tnum = getelementptr %reg_st, %reg_st* %src_reg, i64 0, i32 3
    --> %src_tnum_mask = getelementptr %tnum, %tnum* %src_tnum, i64 0, i32 1
    Resolve where the argument was derived from using GEPMap, and update GEPMap:
    GEPMap: src_tnum_mask: <src_reg, [3, 1]>
    */
    Value *oldGEPMapValue = GEPMap.at(GEPInstArgVal).first;

    std::vector<int> *oldGEPIndices = GEPMap.at(GEPInstArgVal).second;
    for (int i : *oldGEPIndices) {
      GEPIndices->push_back(i);
    }
    valueIndicesPairGEP = std::make_pair(oldGEPMapValue, GEPIndices);

    GEPMap.insert({GEPInstVal, valueIndicesPairGEP});
    populateGEPIndices(GEPInst, *GEPIndices);

  } else {
    throw std::runtime_error(
        "[handleGEPInst]"
        "GEP does not offset into argument derived from "
        "a function argument or a select or a phi or another GEP ");
  }

  outs() << "[handleGEPInst] "
         << "GEPMap: \n";
  printGEPMap();
}

bool loadOrStorePointerCameFromSelect(Value *pointerValue) {

  if (!isa<GetElementPtrInst>(pointerValue)) {
    throw std::runtime_error(
        "[loadOrStorePointerCameFromSelect] load/store "
        "pointer did not come from a GEP, this is not supported.\n");
  }
  GetElementPtrInst &GEPInst = *dyn_cast<GetElementPtrInst>(pointerValue);
  outs() << "[loadOrStorePointerCameFromSelect] the GEP inst:" << GEPInst
         << "\n";
  outs().flush();
  Value *GEPargVal = GEPInst.getOperand(0);
  outs() << "[loadOrStorePointerCameFromSelect] GEPargVal:" << *GEPargVal
         << "\n";
  outs().flush();
  if (isa<SelectInst>(GEPargVal)) {
    outs() << "[loadOrStorePointerCameFromSelect] "
           << "pointerValue is a GEP, that came from a select\n";
    SelectInst &selectInst = *dyn_cast<SelectInst>(GEPargVal);
    outs() << "[loadOrStorePointerCameFromSelect] the select:" << selectInst
           << "\n";
    return true;
  } else {
    outs() << "[loadOrStorePointerCameFromSelect] "
           << "pointerValue is a GEP, but did NOT come from a select\n";
    return false;
  }
}

bool loadOrStorePointerCameFromPhi(Value *pointerValue) {

  if (!isa<GetElementPtrInst>(pointerValue)) {
    throw std::runtime_error(
        "[loadOrStorePointerCameFromPhi] load/store "
        "pointer did not come from a GEP, this is not supported.\n");
  }
  GetElementPtrInst &GEPInst = *dyn_cast<GetElementPtrInst>(pointerValue);
  outs() << "[loadOrStorePointerCameFromPhi] the GEP inst:" << GEPInst << "\n";
  outs().flush();
  Value *GEPargVal = GEPInst.getOperand(0);
  outs() << "[loadOrStorePointerCameFromPhi] GEPargVal:" << *GEPargVal << "\n";
  outs().flush();
  if (isa<PHINode>(GEPargVal)) {
    outs() << "[loadOrStorePointerCameFromPhi] "
           << "pointerValue is a GEP, that came from a phi\n";
    PHINode &phiNode = *dyn_cast<PHINode>(GEPargVal);
    outs() << "[loadOrStorePointerCameFromPhi] the select:" << phiNode << "\n";
    return true;
  } else {
    outs() << "[loadOrStorePointerCameFromPhi] "
           << "pointerValue is a GEP, but did NOT come from a phi\n";
    return false;
  }
}

/*
The details are very similar to handleLoadFromSelect.

Consider the following instruction sequence:
%phi_reg = phi %reg_st* [ %dst_reg, %lfalse ], [ %src_reg, %ltrue]
%phi_reg_tnum_mask = getelementptr %reg_st,
                     %reg_st* %phi_reg, i64 0, i32 3, i32 1
%phi_reg_tnum_mask_load = load i64, i64* %phi_reg_tnum_mask, align 8

- On encountering the phi instruction, the PhiMap and the PhiResolutionMap are
updated:
PhiMap: phi_reg, <dst_reg, lfalse>, <src_reg, ltrue>
PhiResolutionMap:
<ltrue, lend>: ltrue_lend_1_90
<lfalse, lend>: lfalse_lend_1_88

- On encountering the GEP instruciton, the GEPMap is updated:
GEPMap: phi_reg_tnum_mask, phi_reg, 3, 1

- Finally, on encountering the load instruction, each incoming basic block to
the phi node is resolved into its own encoding using GEPMap, PhiMap, and
PhiResolutionMap. We make the following assertions for the current basic block,
one for each incoming edge to the phi node.

(=> lfalse_lend_1_88 (= phi_reg_tnum_mask_load dst_reg_3_1_bv)
(=> ltrue_lend_1_90 (= phi_reg_tnum_mask_load src_reg_3_1_bv))

*/

void FunctionEncoder::handleLoadFromPhi(LoadInst &loadInst,
                                        GetElementPtrInst &GEP,
                                        PHINode &phiInst) {
  outs() << "[handleLoadFromPhi] "
         << "\n";
  outs() << "[handleLoadFromPhi] printPhiMap: ";
  printPhiMap();
  outs() << "[handleLoadFromPhi] "
         << "\n";
  Value *loadInstValue = dyn_cast<Value>(&loadInst);
  outs() << "[handleLoadFromPhi] "
         << "loadInstValue: " << *loadInstValue << "\n";
  Value *pointerValue = loadInst.getOperand(0);
  outs() << "[handleLoadFromPhi] "
         << "pointerValue: " << *pointerValue << "\n";
  MemoryAccess *oldMemoryAccess =
      currentMemorySSA->getMemoryAccess(&loadInst)->getDefiningAccess();
  outs() << "[handleLoadFromPhi] "
         << "definingAccess: " << *oldMemoryAccess << "\n";

  BasicBlock *phiBB = phiInst.getParent();

  z3::expr loadBV = BitVecHelper::getBitVecSingValType(loadInstValue);
  ValueBVTreeMap oldValueBVTreeMap =
      MemoryAccessValueBVTreeMap.at(oldMemoryAccess);
  outs() << "[handleLoadFromPhi] "
         << "oldValueBVTreeMap:\n";
  printValueBVTreeMap(oldValueBVTreeMap);

  std::vector<int> *GEPMapIndices = GEPMap.at(pointerValue).second;
  outs() << "[handleLoadFromPhi] "
         << "GEPMapIndices: " << stdVectorIntToString(*GEPMapIndices) << "\n";

  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);
  std::vector<ValueBBPair> phiValueBBPairs = PhiMap.at(&phiInst);
  for (auto kv : phiValueBBPairs) {
    Value *phiArgValI = kv.first;
    BasicBlock *phiArgBBI = kv.second;
    BBPair BBPairI = std::make_pair(phiArgBBI, phiBB);
    z3::expr phiConditionBoolI = PhiResolutionMap.at(BBPairI);
    outs() << "[handleLoadFromPhi] "
           << "phiConditionBoolI: " << phiConditionBoolI.to_string().c_str()
           << "\n";

    BVTree *subTreeI;
    BVTree *parentBVTreeI = oldValueBVTreeMap.at(phiArgValI);
    outs() << "[handleLoadFromPhi] "
           << "parentBVTree: " << parentBVTreeI->toString() << "\n";

    if (GEPMapIndices->size() == 1) {
      int idx = GEPMapIndices->at(0);
      subTreeI = parentBVTreeI->getSubTree(idx);
    } else if (GEPMapIndices->size() == 2) {
      int idx0 = GEPMapIndices->at(0);
      int idx1 = GEPMapIndices->at(1);
      subTreeI = parentBVTreeI->getSubTree(idx0)->getSubTree(idx1);
    } else {
      throw std::runtime_error("Unexpected GEPMapIndices size\n");
    }

    z3::expr loadEncodingI =
        z3::implies(phiConditionBoolI, loadBV == subTreeI->bv);
    outs() << "[handleLoadFromPhi] " << loadEncodingI.to_string().c_str()
           << "\n";
    BBAsstVecIter->second.push_back(loadEncodingI);
  }
}

/*
Consider the following instruction sequence:

%select_on_ptrs = select i1 %select_value, %bpf_reg_state* %src_reg,
                  %struct.bpf_reg_state* %dst_reg
%gep_ptr = getelementptr %bpf_reg_state, %bpf_reg_state* %select_on_ptrs,
                  i64 0, i32 0
%load_value = load i32, i32* %gep, align 8

On the first select instruction, we update the SelectMap, to just hold
information about the select. SelectMap: {[%select_ptr, %src_reg, %dst_reg]}

On the GEP instruction, we handle this same as before: we store the details of
the GEP in our usual GEPMap. GEPMap: {[%gep_ptr, %select_on_ptrs, 0, 0]}

The idea is that on encountering the load instruction, we can resolve it to a
bitvector formula, conditioned on the result of the select. We have our memory
view of bitvector trees, let's say it is populated as follows before the load.
MemoryViewMap:
{%src_reg: [s1_bv, s2_bv, ...]
%dst_reg: [d1_bv, d2_bv, ...]
}

When we encounter the load.
* We check that the pointer type of the load came from a select, (not from a GEP
  as it usually does)
* Using GEPMap, we figure out that it is associated with %select_on_ptrs.
* We also see that the GEP indexes into the `0`th element of select_on_ptrs, so
we need to access the 0th bitvector in both the bitvector trees in the
MemoryViewMap (s1_bv, d1_bv).
* Using SelectMap, we figure out that the true case of the select is %src_reg,
and the false case is %dst_reg.
* So the 0th bitvector from src_reg is accessed in the true case, and the 0th
  bitvector from dst_reg is accessed in the false case.
* Using the select instruction, we obtain the bitvector that conditions the
select: select_value_bv

We finally make the following assertions for the current basic block:

(ite (= select_value_bv #b1)
     (= load_value_bv s1_bv)
     (= load_value_bv d1_bv))

*/
void FunctionEncoder::handleLoadFromSelect(LoadInst &loadInst,
                                           GetElementPtrInst &GEP,
                                           SelectInst &selectInst) {
  outs() << "[handleLoadFromSelect] "
         << "\n";
  outs() << "[handleLoadFromSelect] SelectMap: ";
  printSelectMap();
  outs() << "[handleLoadFromSelect] "
         << "\n";
  Value *loadInstValue = dyn_cast<Value>(&loadInst);
  outs() << "[handleLoadFromSelect] "
         << "loadInstValue: " << *loadInstValue << "\n";
  Value *pointerValue = loadInst.getOperand(0);
  outs() << "[handleLoadFromSelect] "
         << "pointerValue: " << *pointerValue << "\n";
  MemoryAccess *oldMemoryAccess =
      currentMemorySSA->getMemoryAccess(&loadInst)->getDefiningAccess();
  outs() << "[handleLoadFromSelect] "
         << "definingAccess: " << *oldMemoryAccess << "\n";

  Value *selectOp1 = selectInst.getOperand(0);
  auto z3ExprSelectOp1 = BitVecHelper::getBitVecSingValType(selectOp1);

  z3::expr loadBV = BitVecHelper::getBitVecSingValType(loadInstValue);
  ValueBVTreeMap oldValueBVTreeMap =
      MemoryAccessValueBVTreeMap.at(oldMemoryAccess);
  outs() << "[handleLoadFromSelect] "
         << "oldValueBVTreeMap:\n";
  printValueBVTreeMap(oldValueBVTreeMap);

  std::vector<int> *GEPMapIndices = GEPMap.at(pointerValue).second;
  outs() << "[handleLoadFromSelect] "
         << "GEPMapIndices: " << stdVectorIntToString(*GEPMapIndices) << "\n";

  /* First*/
  BVTree *subTreeFirst = nullptr;
  Value *SelectMapValueFirst = SelectMap.at(&selectInst).first;
  outs() << "[handleLoadFromSelect] "
         << "SelectMapValues: " << SelectMapValueFirst->getName() << "\n";
  BVTree *parentBVTreeFirst = oldValueBVTreeMap.at(SelectMapValueFirst);
  outs() << "[handleLoadFromSelect] "
         << "parentBVTree: " << parentBVTreeFirst->toString() << "\n";

  if (GEPMapIndices->size() == 1) {
    int idx = GEPMapIndices->at(0);
    subTreeFirst = parentBVTreeFirst->getSubTree(idx);
  } else if (GEPMapIndices->size() == 2) {
    int idx0 = GEPMapIndices->at(0);
    int idx1 = GEPMapIndices->at(1);
    subTreeFirst = parentBVTreeFirst->getSubTree(idx0)->getSubTree(idx1);
  } else {
    throw std::runtime_error("Unexpected GEPMapIndices size\n");
  }

  /* Second*/
  Value *SelectMapValueSecond = SelectMap.at(&selectInst).second;
  outs() << "[handleLoadFromSelect] "
         << "SelectMapValues: " << SelectMapValueSecond->getName() << "\n";
  BVTree *parentBVTreeSecond = oldValueBVTreeMap.at(SelectMapValueSecond);
  outs() << "[handleLoadFromSelect] "
         << "parentBVTree: " << parentBVTreeSecond->toString() << "\n";
  BVTree *subTreeSecond;
  if (GEPMapIndices->size() == 1) {
    int idx = GEPMapIndices->at(0);
    subTreeSecond = parentBVTreeSecond->getSubTree(idx);
  } else if (GEPMapIndices->size() == 2) {
    int idx0 = GEPMapIndices->at(0);
    int idx1 = GEPMapIndices->at(1);
    subTreeSecond = parentBVTreeSecond->getSubTree(idx0)->getSubTree(idx1);
  } else {
    throw std::runtime_error("Unexpected GEPMapIndices size\n");
  }

  z3::expr loadEncoding =
      (z3::ite((z3ExprSelectOp1 == 1), (loadBV == subTreeFirst->bv),
               (loadBV == subTreeSecond->bv)));

  outs() << "[handleLoadInst] " << loadEncoding.to_string().c_str() << "\n";

  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);
  BBAsstVecIter->second.push_back(loadEncoding);
  printBBAssertionsMap();
  outs().flush();
}

void FunctionEncoder::handleLoadInst(LoadInst &i) {
  outs() << "[handleLoadInst] "
         << "\n";
  Value *loadInstValue = dyn_cast<Value>(&i);
  outs() << "[handleLoadInst] "
         << "loadInstValue: " << *loadInstValue << "\n";
  Value *pointerValue = i.getOperand(0);
  outs() << "[handleLoadInst] "
         << "pointerValue: " << *pointerValue << "\n";
  Type *pointerType = pointerValue->getType();
  outs() << "[handleLoadInst] "
         << "pointerType: " << *pointerType << "\n";
  Type *pointerBaseType = pointerType->getPointerElementType();
  outs() << "[handleLoadInst] "
         << "pointerBaseType: " << *pointerBaseType << "\n";
  MemoryAccess *oldMemoryAccess =
      currentMemorySSA->getMemoryAccess(&i)->getDefiningAccess();
  outs() << "[handleLoadInst] "
         << "definingAccess: " << *oldMemoryAccess << "\n";

  /* Loads a pointer to pointer, treat this like a GEP instruction */
  if (pointerBaseType->isPointerTy()) {
    throw std::runtime_error("[handleCallInst]"
                             "load instruction with pointer that is a pointer "
                             "to a pointer is not supported\n");
  }

  if (loadInstValue->getType()->isAggregateType()) {
    throw std::runtime_error("[handleCallInst]"
                             "load instruction with pointer that is a pointer "
                             "to an aggregate type is not supported\n");
  }

  outs().flush();

  if (loadOrStorePointerCameFromSelect(pointerValue)) {
    GetElementPtrInst &GEPInst = *dyn_cast<GetElementPtrInst>(pointerValue);
    Value *GEPargVal = GEPInst.getOperand(0);
    SelectInst &selectInst = *dyn_cast<SelectInst>(GEPargVal);
    handleLoadFromSelect(i, GEPInst, selectInst);
    return;
  } else if (loadOrStorePointerCameFromPhi(pointerValue)) {
    GetElementPtrInst &GEPInst = *dyn_cast<GetElementPtrInst>(pointerValue);
    Value *GEPargVal = GEPInst.getOperand(0);
    PHINode &phiNode = *dyn_cast<PHINode>(GEPargVal);
    handleLoadFromPhi(i, GEPInst, phiNode);
    return;
  }

  outs() << "[handleLoadInst] "
         << "GEPMap:\n";
  printGEPMap();
  z3::expr loadBV = BitVecHelper::getBitVecSingValType(loadInstValue);
  ValueBVTreeMap oldValueBVTreeMap =
      MemoryAccessValueBVTreeMap.at(oldMemoryAccess);
  outs() << "[handleLoadInst] "
         << "oldValueBVTreeMap:\n";
  printValueBVTreeMap(oldValueBVTreeMap);

  Value *GEPMapValue = GEPMap.at(pointerValue).first;
  outs() << "[handleLoadInst] "
         << "GEPMapValue: " << GEPMapValue->getName() << "\n";
  assert(FunctionArgs.find(GEPMapValue) != FunctionArgs.end());

  std::vector<int> *GEPMapIndices = GEPMap.at(pointerValue).second;
  outs() << "[handleLoadInst] "
         << "GEPMapIndices: " << stdVectorIntToString(*GEPMapIndices) << "\n";

  BVTree *parentBVTree = oldValueBVTreeMap.at(GEPMapValue);
  outs() << "[handleLoadInst] "
         << "parentBVTree: " << parentBVTree->toString() << "\n";
  BVTree *subTree;
  if (GEPMapIndices->size() == 1) {
    int idx = GEPMapIndices->at(0);
    subTree = parentBVTree->getSubTree(idx);
  } else if (GEPMapIndices->size() == 2) {
    int idx0 = GEPMapIndices->at(0);
    int idx1 = GEPMapIndices->at(1);
    subTree = parentBVTree->getSubTree(idx0)->getSubTree(idx1);
  } else {
    throw std::runtime_error("Unexpected GEPMapIndices size\n");
  }
  assert(subTree->bv);

  z3::expr loadEncoding = (subTree->bv == loadBV);
  outs() << "[handleLoadInst] " << loadEncoding.to_string().c_str() << "\n";
  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);
  BBAsstVecIter->second.push_back(loadEncoding);
  printBBAssertionsMap();
}

#if 0

%struct.bpf_reg_state = type { i32, i64, i64, i64, i64}

define dso_local void @foo(%struct.bpf_reg_state* %dst_reg, %struct.bpf_reg_state* %src_reg) local_unnamed_addr align 16 {
entry:
  %umin_src = getelementptr inbounds %struct.bpf_reg_state, %struct.bpf_reg_state* %src_reg, i64 0, i32 1
; MemoryUse(liveOnEntry) MayAlias
  %umin_src_load = load i64, i64* %umin_src, align 8
  %type_src = getelementptr inbounds %struct.bpf_reg_state, %struct.bpf_reg_state* %src_reg, i64 0, i32 0
; MemoryUse(liveOnEntry) MayAlias
  %type_src_load = load i32, i32* %type_src, align 8
  %type_src_is_zero = icmp eq i32 %type_src_load, 0
  br i1 %type_src_is_zero, label %ltrue, label %lend

ltrue:                                            ; preds = %entry
  %smin_dst = getelementptr inbounds %struct.bpf_reg_state, %struct.bpf_reg_state* %dst_reg, i64 0, i32 3
; 1 = MemoryDef(liveOnEntry)
  store i64 0, i64* %smin_dst, align 8
  br label %lend

lend:                                             ; preds = %ltrue, %entry
; 3 = MemoryPhi({entry,liveOnEntry},{ltrue,1})
  %phi_reg = phi %struct.bpf_reg_state* [ %dst_reg, %ltrue ], [ %src_reg, %entry ]
  %smin_phi = getelementptr inbounds %struct.bpf_reg_state, %struct.bpf_reg_state* %phi_reg, i64 0, i32 3
; 2 = MemoryDef(3)
  store i64 %umin_src_load, i64* %smin_phi, align 8
  ret void
}

#endif

/*
Consider the above function foo. Before we encounter the store instruction, we
have: ; GEPMap: {{umin, src_reg, [1]},{type_src, src_reg, [0],{smin_dst,
dst_reg, [3]}, {smin_phi, phi_reg, [3]}}

MemoryViewMap:
(liveOnEntry): {
%src_reg: [s0_bv, s1_bv, s2_bv, s3_bv, s4_bv]
%dst_reg: [d0_bv, d1_bv, d2_bv, d3_bv, d4_bv]
}

1 = MemoryDef(liveOnEntry) : {
%src_reg: [s0_bv, s1_bv, s2_bv, 0x0, s4_bv]
%dst_reg: [d0_bv, d1_bv, d2_bv, d3_bv, d4_bv]
}

3 = MemoryPhi({entry,liveOnEntry},{ltrue,1}): {
%src_reg: [s0_p_bv, s1_p_bv, s2_p_bv, s3_p_bv, s4_p_bv]
%dst_reg: [d0_p_bv, d1_p_bv, d2_p_bv, d3_p_bv, d4_p_bv]
}

PhiMap:
phi_reg : {<dst_reg, ltrue>, <src_reg, entry>}

PhiResolutionMap:
{{<ltrue, lend>: ltrue_lend_pc},{<entry, lend>: entry_lend_pc}}

---

To encode the store, we first create a copy of the MemoryDef the
store modifies. In this case,

; 2 = MemoryDef(3)
; %src_reg: [s0_p_bv, s1_p_bv, s2_p_bv, phi_resolve_bv_1, s4_p_bv]
; %dst_reg: [d0_p_bv, d1_p_bv, d2_p_bv, phi_resolve_bv_2, d4_p_bv]

Then we encode the store:
  ; (ite (= entry_lend_pc #b1)
  ;      (= phi_resolve_bv_1 umin_src_load_bv)
  ;      (= phi_resolve_bv_1 s3_p_bv))
  ; (ite (= ltrue_lend_pc #b1)
  ;      (= phi_resolve_bv_2 umin_src_load_bv)
  ;      (= phi_resolve_bv_1 d3_p_bv))

*/

void FunctionEncoder::handleStoreFromPhi(StoreInst &storeInst,
                                         GetElementPtrInst &GEPInst,
                                         PHINode &phiInst,
                                         ValueBVTreeMap *newValueBVTreeMap) {
  outs() << "[handleStoreFromPhi] "
         << "\n";

  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);

  Value *valueToStore = storeInst.getValueOperand();
  z3::expr z3ExprToStore = BitVecHelper::getBitVecSingValType(valueToStore);

  /* get MemoryAccess corresponding to this store */
  MemoryUseOrDef *storeMemoryAccess =
      currentMemorySSA->getMemoryAccess(&storeInst);
  outs() << "[handleStoreFromPhi] "
         << "storeMemoryAccess: " << *storeMemoryAccess << "\n";
  assert(isa<MemoryDef>(storeMemoryAccess));

  outs() << "[handleStoreFromPhi] "
         << "newValueBVTreeMap (copied from oldValueBVTreeMap):\n"
         << ValueBVTreeMapToString(*newValueBVTreeMap);

  BasicBlock *phiBB = phiInst.getParent();
  std::vector<ValueBBPair> phiValueBBPairs = PhiMap.at(&phiInst);

  for (auto kv : phiValueBBPairs) {
    Value *phiArgValI = kv.first;
    BasicBlock *phiArgBBI = kv.second;

    BVTree *parentBVTreeI = newValueBVTreeMap->at(phiArgValI);
    outs() << "[handleStoreFromPhi] "
           << "parentBVTree1:\n";
    outs() << parentBVTreeI->toString() << "\n";

    z3::expr phiStoreResolveBVI = BitVecHelper::getBitVec(
        valueToStore->getType()->getIntegerBitWidth(), "phi_resolve_");

    std::vector<int> *GEPMapIndicesI =
        GEPMap.at(storeInst.getPointerOperand()).second;
    outs() << "[handleStoreFromPhi] "
           << "GEPMapIndices: " << stdVectorIntToString(*GEPMapIndicesI)
           << "\n";
    outs().flush();

    BVTree *subTreeI;
    if (GEPMapIndicesI->size() == 1) {
      int idx = GEPMapIndicesI->at(0);
      subTreeI = parentBVTreeI->getSubTree(idx);
    } else if (GEPMapIndicesI->size() == 2) {
      int idx0 = GEPMapIndicesI->at(0);
      int idx1 = GEPMapIndicesI->at(1);
      subTreeI = parentBVTreeI->getSubTree(idx0)->getSubTree(idx1);
    } else {
      throw std::runtime_error(
          "[handleStoreFromPhi]: Unexpected GEPMapIndices size\n");
    }

    outs() << "[handleStoreFromPhi] "
           << "subTreeI: " << subTreeI->toString() << "\n";
    z3::expr oldStoreBVI = subTreeI->bv;
    assert(oldStoreBVI);
    subTreeI->bv = phiStoreResolveBVI;
    outs() << "[handleStoreFromPhi] "
           << "subTreeI updated: " << subTreeI->toString() << "\n";

    // ------------------------------------------------------------------
    BBPair BBPairI = std::make_pair(phiArgBBI, phiBB);
    z3::expr phiConditionBoolI = PhiResolutionMap.at(BBPairI);
    outs() << "[handleStoreFromPhi] "
           << "phiConditionBoolI: " << phiConditionBoolI.to_string().c_str()
           << "\n";
    outs() << "[handleStoreFromPhi] "
           << "phiConditionBoolI sort"
           << phiConditionBoolI.get_sort().to_string().c_str() << "\n";
    outs().flush();

    z3::expr storeFromPhiEncodingI =
        z3::ite(phiConditionBoolI, phiStoreResolveBVI == z3ExprToStore,
                phiStoreResolveBVI == oldStoreBVI);
    outs() << "[handleStoreFromPhi] "
           << "storeFromPhiEncodingI: " << storeFromPhiEncodingI.to_string()
           << "\n";

    BBAsstVecIter->second.push_back(storeFromPhiEncodingI);
  }

  MemoryAccessValueBVTreeMap.insert({storeMemoryAccess, *newValueBVTreeMap});
  mostRecentMemoryDef = storeMemoryAccess;
  outs() << "[handleStoreInst] "
         << "MemoryAccessValueBVTreeMap:\n";
  printMemoryAccessValueBVTreeMap();
  printBBAssertionsMap(currentBB);
  outs().flush();
}

#if 0
%struct.reg_st = type { i64, i64, i64, i64, i64, i64, i64, i64}
; Function Attrs: noinline nounwind uwtable
define dso_local void @foo(%struct.reg_st* %dst_reg, %struct.reg_st* %src_reg) local_unnamed_addr #0 align 16 {
  %umin = getelementptr inbounds %struct.reg_st, %struct.reg_st* %src_reg, i64 0, i32 4
  %umin_load = load i64, i64* %umin, align 8
  %umin_is_zero = icmp eq i64 %umin_load, 0
  %select_false_reg = select i1 %umin_is_zero, %struct.reg_st* %src_reg, %struct.reg_st* %dst_reg
  %umax_false_reg = getelementptr inbounds %struct.reg_st, %struct.reg_st* %select_false_reg, i64 0, i32 5
  store i64 %umin_load, i64* %umax_false_reg, align 8
  ret void
}
#endif

/*
Consider the above function foo. With the following MemoryViewMap on entry:
(liveOnEntry):
{%src_reg: [s0_bv, s1_bv, s2_bv, s3_bv, s4_bv, s5_bv]
%dst_reg: [d0_bv, d1_bv, d2_bv, d3_bv, d4_bv, d5_bv]
}

Before we encounter the 2nd GEP, we have:
GEPMap: umin, src_reg, [4]
(= s4_bv umin_load_bv) ; from load
(ite (= umin_load_bv #x0000000000000000) ; from icmp
     (= umin_is_zero_bv #b1)
     (= umin_is_zero_bv #b0))
SelectMap: select_false_reg,src_reg,dst_reg

On the 2nd GEP, we update GEPMap:
umax_false_reg, select_false_reg, [5]
umin, src_reg, [4]

Now, we encouter the store. We see that the store instruction is storing to
a pointer that comes from a select on pointers.
* Because this is a store that modifies the MemoryViewMap, we first make a
copy of the defining MemoryViewMap (liveOnEntry) to associate with the
MemoryDef (1) corresponding to this store. We need to modify this
MemoryViewMap.
* We create two new bitvectors that we will store in our MemoryViewMap. Each
of these bitvectors will be stored in the index obtained from the previous
GEP (index 5).

MemoryViewMap:
1 = MemoryDef(liveOnEntry):
{%src_reg: [s0_bv, s1_bv, s2_bv, s3_bv, s4_bv, select_resolve_bv_1]
%dst_reg: [d0_bv, d1_bv, d2_bv, d3_bv, d4_bv, select_resolve_bv_2]
}

* We now need to assert that the MemoryViewMap at each location (src_reg[5]
and dst_reg[5]) changes, if the boolean of the comparison that the select was
based on is true. Else it is unchanged. That is if umin_is_zero is true,
src_reg[5] will be updated, else it will remain unchanged. And if umin_is_zero
is false, dst_reg[5] will be updated, else it will remain unchanged.

(ite (= umin_is_zero_bv #b1)
     (= select_resolve_bv_1 umin_load_bv)
     (= select_resolve_bv_1 s5_bv))
(ite (= umin_is_zero_bv #b0)
    (= select_resolve_bv_2 umin_load_bv)
    (= select_resolve_bv_2 d5_bv))
*/

void FunctionEncoder::handleStoreFromSelect(StoreInst &storeInst,
                                            GetElementPtrInst &GEPInst,
                                            SelectInst &selectInst,
                                            ValueBVTreeMap *newValueBVTreeMap) {
  outs() << "[handleStoreFromSelect] "
         << "\n";
  outs() << storeInst << "\n";
  outs() << GEPInst << "\n";
  outs() << selectInst << "\n";
  outs().flush();

  Value *selectConditionOp = selectInst.getOperand(0);
  auto z3ExprSelectConditionOp =
      BitVecHelper::getBitVecSingValType(selectConditionOp);

  auto BBAsstVecIter = BBAssertionsMap.find(currentBB);

  Value *valueStored = storeInst.getValueOperand();
  z3::expr z3ExprStoreValue = BitVecHelper::getBitVecSingValType(valueStored);

  /* get MemoryAccess corresponding to this store */
  MemoryUseOrDef *storeMemoryAccess =
      currentMemorySSA->getMemoryAccess(&storeInst);
  outs() << "[handleStoreFromSelect] "
         << "storeMemoryAccess: " << *storeMemoryAccess << "\n";
  assert(isa<MemoryDef>(storeMemoryAccess));

  outs() << "[handleStoreFromSelect] "
         << "newValueBVTreeMap (copied from oldValueBVTreeMap):\n"
         << ValueBVTreeMapToString(*newValueBVTreeMap);

  // ------------------------------------------------------------------

  /* First */
  Value *selectInstOp1 = selectInst.getOperand(1);
  outs() << "[handleStoreFromSelect] "
         << "selectInstOp1:" << *selectInstOp1 << '\n';
  BVTree *parentBVTree1 = newValueBVTreeMap->at(selectInstOp1);
  outs() << "[handleStoreFromSelect] "
         << "parentBVTree1:\n";
  outs() << parentBVTree1->toString() << "\n";

  z3::expr selectStoreResolveBV1 = BitVecHelper::getBitVec(
      valueStored->getType()->getIntegerBitWidth(), "select_resolve_");

  std::vector<int> *GEPMapIndices1 =
      GEPMap.at(storeInst.getPointerOperand()).second;
  outs() << "[handleStoreFromSelect] "
         << "GEPMapIndices: " << stdVectorIntToString(*GEPMapIndices1) << "\n";
  outs().flush();

  BVTree *subTree1;
  if (GEPMapIndices1->size() == 1) {
    int idx = GEPMapIndices1->at(0);
    subTree1 = parentBVTree1->getSubTree(idx);
  } else if (GEPMapIndices1->size() == 2) {
    int idx0 = GEPMapIndices1->at(0);
    int idx1 = GEPMapIndices1->at(1);
    subTree1 = parentBVTree1->getSubTree(idx0)->getSubTree(idx1);
  } else {
    throw std::runtime_error(
        "[handleStoreFromSelect]: Unexpected GEPMapIndices size\n");
  }

  outs() << "[handleStoreFromSelect] "
         << "subTree1: " << subTree1->toString() << "\n";
  z3::expr oldStoreBV1 = subTree1->bv;
  assert(oldStoreBV1);
  subTree1->bv = selectStoreResolveBV1;
  outs() << "[handleStoreFromSelect] "
         << "subTree1 updated: " << subTree1->toString() << "\n";

  z3::expr storeFromSelectEncoding1 = z3::ite(
      z3ExprSelectConditionOp == 1, selectStoreResolveBV1 == z3ExprStoreValue,
      selectStoreResolveBV1 == oldStoreBV1);

  outs() << "[handleStoreFromSelect] "
         << "storeFromSelectEncoding1: " << storeFromSelectEncoding1.to_string()
         << "\n";

  BBAsstVecIter->second.push_back(storeFromSelectEncoding1);

  // ------------------------------------------------------------------

  /* Second */
  Value *selectInstOp2 = selectInst.getOperand(2);
  BVTree *parentBVTree2 = newValueBVTreeMap->at(selectInstOp2);
  outs() << parentBVTree2->toString() << "\n";

  z3::expr selectStoreResolveBV2 = BitVecHelper::getBitVec(
      valueStored->getType()->getIntegerBitWidth(), "select_resolve_");

  std::vector<int> *GEPMapIndices2 =
      GEPMap.at(storeInst.getPointerOperand()).second;
  outs() << "[handleStoreFromSelect] "
         << "GEPMapIndices: " << stdVectorIntToString(*GEPMapIndices2) << "\n";
  outs().flush();

  BVTree *subTree2;
  if (GEPMapIndices2->size() == 1) {
    int idx = GEPMapIndices2->at(0);
    subTree2 = parentBVTree2->getSubTree(idx);
  } else if (GEPMapIndices2->size() == 2) {
    int idx0 = GEPMapIndices2->at(0);
    int idx1 = GEPMapIndices2->at(1);
    subTree2 = parentBVTree2->getSubTree(idx0)->getSubTree(idx1);
  } else {
    throw std::runtime_error("Unexpected GEPMapIndices size\n");
  }

  outs() << "[handleStoreFromSelect] "
         << "subTree2: " << subTree2->toString() << "\n";
  z3::expr oldStoreBV2 = subTree2->bv;
  assert(oldStoreBV2);
  subTree2->bv = selectStoreResolveBV2;
  outs() << "[handleStoreFromSelect] "
         << "subTree2 updated: " << subTree2->toString() << "\n";

  z3::expr storeFromSelectEncoding2 = z3::ite(
      z3ExprSelectConditionOp == 0, selectStoreResolveBV2 == z3ExprStoreValue,
      selectStoreResolveBV2 == oldStoreBV2);

  outs() << "[handleStoreFromSelect] "
         << "storeFromSelectEncoding2: " << storeFromSelectEncoding2.to_string()
         << "\n";

  BBAsstVecIter->second.push_back(storeFromSelectEncoding2);

  MemoryAccessValueBVTreeMap.insert({storeMemoryAccess, *newValueBVTreeMap});
  mostRecentMemoryDef = storeMemoryAccess;
  outs() << "[handleStoreFromSelect] "
         << "MemoryAccessValueBVTreeMap:\n";
  printMemoryAccessValueBVTreeMap();
  printBBAssertionsMap(currentBB);

  outs().flush();
  // exit(0);
}

void FunctionEncoder::handleStoreInst(StoreInst &i) {

  /* Syntax:
   * store i64 %value, i64* %dest
   */

  outs() << "[handleStoreInst] "
         << "\n";
  Value *valueStored = i.getValueOperand();
  z3::expr BVToStore = BitVecHelper::getBitVecSingValType(valueStored);
  outs() << "[handleStoreInst] "
         << "BVToStore: " << BVToStore.to_string() << "\n";

  /* Get location where value is stored */
  Value *destPointerValue = i.getPointerOperand();
  outs() << "[handleStoreInst] "
         << "destPointerValue: " << destPointerValue->getName() << "\n";
  outs().flush();

  /* get MemoryAccess corresponding to this store */
  MemoryUseOrDef *storeMemoryAccess = currentMemorySSA->getMemoryAccess(&i);
  outs() << "[handleStoreInst] "
         << "storeMemoryAccess: " << *storeMemoryAccess << "\n";
  assert(isa<MemoryDef>(storeMemoryAccess));

  /* Get original MemoryAccess that this store eventually modifies */
  MemoryAccess *oldMemoryAccess = storeMemoryAccess->getDefiningAccess();
  outs() << "[handleStoreInst] "
         << "definingMemoryAccess: " << *oldMemoryAccess << "\n";

  /* Get ValueBVTreeMap corresponding to orignal MemoryAccess */
  ValueBVTreeMap oldValueBVTreeMap =
      MemoryAccessValueBVTreeMap.at(oldMemoryAccess);

  ValueBVTreeMap newValueBVTreeMap;
  /* First copy entire oldValueBVTreeMap to newValueBVTreeMap */
  for (std::pair<Value *, BVTree *> kv : oldValueBVTreeMap) {
    Value *v1 = kv.first;
    BVTree *oldTree = kv.second;
    BVTree *newTree = oldTree->copy();
    newValueBVTreeMap.insert({v1, newTree});
  }
  outs() << "[handleStoreInst] "
         << "newValueBVTreeMap (copied from oldValueBVTreeMap):\n"
         << ValueBVTreeMapToString(newValueBVTreeMap);

  if (loadOrStorePointerCameFromSelect(destPointerValue)) {
    GetElementPtrInst &GEPInst = *dyn_cast<GetElementPtrInst>(destPointerValue);
    Value *GEPargVal = GEPInst.getOperand(0);
    SelectInst &selectInst = *dyn_cast<SelectInst>(GEPargVal);
    handleStoreFromSelect(i, GEPInst, selectInst, &newValueBVTreeMap);
    return;
  }

  if (loadOrStorePointerCameFromPhi(destPointerValue)) {
    GetElementPtrInst &GEPInst = *dyn_cast<GetElementPtrInst>(destPointerValue);
    Value *GEPargVal = GEPInst.getOperand(0);
    PHINode &phiInst = *dyn_cast<PHINode>(GEPargVal);
    handleStoreFromPhi(i, GEPInst, phiInst, &newValueBVTreeMap);
    return;
  }

  Value *GEPMapValue = GEPMap.at(destPointerValue).first;
  outs() << "[handleStoreInst] "
         << "GEPMapValue: " << GEPMapValue->getName() << "\n";

  std::vector<int> *GEPMapIndices = GEPMap.at(destPointerValue).second;
  outs() << "[handleStoreInst] "
         << "GEPMapIndices: " << stdVectorIntToString(*GEPMapIndices) << "\n";
  outs().flush();

  BVTree *parentBVTree = newValueBVTreeMap.at(GEPMapValue);
  BVTree *subTree;
  if (GEPMapIndices->size() == 1) {
    int idx = GEPMapIndices->at(0);
    subTree = parentBVTree->getSubTree(idx);
  } else if (GEPMapIndices->size() == 2) {
    int idx0 = GEPMapIndices->at(0);
    int idx1 = GEPMapIndices->at(1);
    subTree = parentBVTree->getSubTree(idx0)->getSubTree(idx1);
  } else {
    throw std::runtime_error("Unexpected GEPMapIndices size\n");
  }

  outs() << "[handleStoreInst] "
         << "subTree: " << subTree->toString() << "\n";
  assert(subTree->bv);
  subTree->bv = BVToStore;
  outs() << "[handleStoreInst] "
         << "subTree updated: " << subTree->toString() << "\n";

  MemoryAccessValueBVTreeMap.insert({storeMemoryAccess, newValueBVTreeMap});
  mostRecentMemoryDef = storeMemoryAccess;
  outs() << "[handleStoreInst] "
         << "MemoryAccessValueBVTreeMap:\n";
  printMemoryAccessValueBVTreeMap();
}

/* Note: variables defined in the given BB are visible from all BBs that are
 * dominated by the given one. */

/* TODO:
Nice to have: make handleMemoryPhiNode similar in structure to handlePhiNode.
Get rid of MemoryPhiResolutionMap.

In pass 1: Instead create a boolean z3 variable for each incoming edge to the
phi (e.g. bool_1, bool_2). Then immediately add to BBAssertions map:
bool_1 ==> (conjunction of all BBExprsI)
bool_2 ==> (conjunction of all BBExprsI)

In pass 3: bool_1 and bool_2 will be set according to EdgeAssertionsMap.
*/
void FunctionEncoder::handleMemoryPhiNode(MemoryPhi &mphi, int passID) {
  outs() << "[handleMemoryPhiNode] "
         << "Pass #" << passID << "\n";

  mostRecentMemoryDef = &mphi;

  /* In pass 1, we populate MemoryPhiResolutionMap. */
  if (passID == 1) {

    /* Create new Value:BVTree Map for this MemoryPhi node, and populate it
     * with BVTrees for only those Value(s) which are function arguments. */
    ValueBVTreeMap phiBVTreeMap;

    for (auto argIterator = currentFunction->arg_begin();
         argIterator != currentFunction->arg_end(); argIterator++) {
      Value *argVal = dyn_cast<Value>(argIterator);
      Type *argType = argIterator->getType();
      if (!argType->isPointerTy())
        continue;
      BVTree *BVTreeForArg = setupBVTreeForArg(argVal, argVal->getName().str());
      phiBVTreeMap.insert({argVal, BVTreeForArg});
    }

    outs() << "[handleMemoryPhiNode] "
           << "phiBVTreeMap: "
           << "\n";
    printValueBVTreeMap(phiBVTreeMap);

    /* Iterate over all incoming BasicBlocks to a MemoryPhi block and populate
     * MemoryPhiResolutionMap. MemoryPhiResolutionMap contains BBPairs, one
     * pair for each edge incoming to the memoryPhi BasicBlock. Associated to
     * each BBpair is a list of z3 expressions that assert the bitvector
     * equivalences for that particular edge. */
    for (auto i = 0u; i < mphi.getNumIncomingValues(); i++) {

      BasicBlock *incomingBBI = mphi.getIncomingBlock(i);
      outs() << "[handleMemoryPhiNode] "
             << "incomingBBI: " << incomingBBI->getName() << "\n";
      BBPair BBPairI = std::make_pair(incomingBBI, currentBB);
      MemoryAccess *incomingAccessI = mphi.getIncomingValue(i);
      outs() << "[handleMemoryPhiNode] "
             << "incomingAccessI: " << *incomingAccessI << "\n";
      ValueBVTreeMap oldVBVTreeMapI =
          MemoryAccessValueBVTreeMap.at(incomingAccessI);
      outs() << "[handleMemoryPhiNode] "
             << "oldVBVTreeMapI: "
             << "\n";
      printValueBVTreeMap(oldVBVTreeMapI);

      z3::expr_vector BBExprsI(ctx);
      MemoryPhiResolutionMap.insert({BBPairI, BBExprsI});

      for (std::pair<Value *, BVTree *> kv : phiBVTreeMap) {
        Value *argValue = kv.first;
        if (oldVBVTreeMapI.find(argValue) != oldVBVTreeMapI.end()) {
          BVTree *oldTreeArgI = oldVBVTreeMapI.at(argValue);
          outs() << "[handleMemoryPhiNode] "
                 << "oldTreeArgI: " << oldTreeArgI->toString() << "\n";
          BVTree *phiTreeArgI = phiBVTreeMap.at(argValue);
          outs() << "[handleMemoryPhiNode] "
                 << "phiTreeArgI: " << phiTreeArgI->toString() << "\n";
          BVTree::getEquivVector(oldTreeArgI, phiTreeArgI, BBExprsI);
        }
      }
      outs() << "[handleMemoryPhiNode] "
             << "getEquivVector: " << Z3ExprVecToString(BBExprsI) << "\n";
      /* Finally insert the phiBVTreeMap into the global
       * MemoryAccessValueBVTreeMap. */
      MemoryAccessValueBVTreeMap.insert({&mphi, phiBVTreeMap});
    }
    outs() << "[handleMemoryPhiNode] "
           << "MemoryPhiResolutionMap:\n";
    printMemoryPhiResolutionMap();

    /* In pass 3, we have the path conditions for each BBPair populated in
     * EdgeAssertionsMap. Use them, and the MemoryPhiResolutionMap, to figure
     * what is the z3 expressions formed as a result of taking the particular
     * edge.
     */
  } else if (passID == 3) {

    auto BBAsstVecIter = BBAssertionsMap.find(currentBB);

    for (auto i = 0u; i < mphi.getNumIncomingValues(); i++) {
      BasicBlock *incomingBBI = mphi.getIncomingBlock(i);
      BBPair BBPairI = std::make_pair(incomingBBI, currentBB);
      assert(EdgeAssertionsMap.find(BBPairI) != EdgeAssertionsMap.end());
      z3::expr_vector BBExprsI = MemoryPhiResolutionMap.at(BBPairI);
      z3::expr phiResolveI =
          z3::implies(EdgeAssertionsMap.at(BBPairI), z3::mk_and(BBExprsI));
      outs() << "phiResolveI (i=" << i << ") "
             << phiResolveI.to_string().c_str() << "\n";
      BBAsstVecIter->second.push_back(phiResolveI);
    }
  }
}

void FunctionEncoder::handleCallInst(CallInst &i) {
  throw std::runtime_error("[handleCallInst]"
                           "call instruction not supported\n");
}

/*
Go instruction by instruction in the BasicBlock, and build BBAssertionsMap
 */
void FunctionEncoder::populateBBAssertionsMap(BasicBlock &B) {
  for (Instruction &I : B) {
    FunctionEncoder::currentInstruction = &I;
    outs() << "-------------------\n"
           << I << "\n"
           << "-------------------\n";
    if (I.isDebugOrPseudoInst()) {
      continue;
    }
    if (isa<BinaryOperator>(&I)) {
      handleBinaryOperatorInst(*(dyn_cast<BinaryOperator>(&I)));
    } else if (isa<CastInst>(&I)) {
      handleCastInst(*(dyn_cast<CastInst>(&I)));
    } else if (isa<ICmpInst>(&I)) {
      handleICmpInst(*(dyn_cast<ICmpInst>(&I)));
    } else if (isa<SelectInst>(&I)) {
      handleSelectInst(*(dyn_cast<SelectInst>(&I)));
    } else if (isa<PHINode>(&I)) {
      handlePhiNode(*(dyn_cast<PHINode>(&I)), 1);
    } else if (isa<GetElementPtrInst>(&I)) {
      handleGEPInst(*dyn_cast<GetElementPtrInst>(&I));
    } else if (isa<LoadInst>(&I)) {
      handleLoadInst(*dyn_cast<LoadInst>(&I));
    } else if (isa<StoreInst>(&I)) {
      handleStoreInst(*dyn_cast<StoreInst>(&I));
    } else if (isa<CallInst>(&I)) {
      handleCallInst(*dyn_cast<CallInst>(&I));
    } else {
      outs() << "Unknown instruction. Continuing...\n";
      continue;
    }
  }
}

void FunctionEncoder::populatePathConditionsMap(BasicBlock &B) {
  for (Instruction &I : B) {
    outs() << "-------------------\n"
           << I << "\n"
           << "-------------------\n";
    if (isa<BranchInst>(&I)) {
      handleBranchInst(*(dyn_cast<BranchInst>(&I)));
    } else {
      continue;
    }
  }
}

bool FunctionEncoder::isRelevantStruct(StructType *s) {
  outs() << "[isRelevantStruct] " << *s << "\n";
  if (s->isLiteral()) {
    outs() << "[isRelevantStruct] "
           << "literal struct, this is relevant\n";
    return true;
  }
  std::string structName = s->getStructName().str();
  auto relevant =
      std::find(std::begin(FunctionEncoder::relevantStructs),
                std::end(FunctionEncoder::relevantStructs),
                structName) != std::end(FunctionEncoder::relevantStructs);
  outs() << "[isRelevantStruct] " << structName << " " << relevant << "\n";
  return relevant;
}

/* Recursive function that looks inside structs, and converts them into
 * BVTrees (a tree of bitvectors that mimics the shape of the struct) */
void FunctionEncoder::convertAggregateTypeArgToBVTree(
    BVTree *parent, StructType *baseStructType, std::string prefix) {

  /* Iterate over struct internal members */
  for (auto internalTypeIterator = baseStructType->subtype_begin();
       internalTypeIterator != baseStructType->subtype_end();
       internalTypeIterator++) {
    Type *internalType = *internalTypeIterator;
    outs() << "[convertAggregateTypeArgToBVTree] "
           << "internalType: " << *internalType << "\n";

    while (internalType->isPointerTy()) {
      /* Treat pointers as their base type */
      internalType = internalType->getPointerElementType();
      outs() << "[convertAggregateTypeArgToBVTree] "
             << "internalType (updated): " << *internalType << "\n";
    }

    if (internalType == baseStructType) { /* This is a linked list pointer */
      BVTree *emptyTree = new BVTree();
      parent->children.push_back(emptyTree);
      outs() << "[convertAggregateTypeArgToBVTree] "
             << "pointer to same type, continuing\n";
      continue;
    }

    /* TODO: if array types are need to be supported, change the ifdef to 1.
     * Arrays will also need to be supported in creating JSON dictionaries
     * in getJsonDictFromValueBVTree() and friends. */
    assert(!internalType->isArrayTy());

#if 0
    if (internalType->isArrayTy()) {
      ArrayType *arrayType = dyn_cast<ArrayType>(internalType);
      auto arrayInternalType = arrayType->getElementType();
      outs() << "[convertAggregateTypeArgToBVTree] "
             << "arrayInteralType " << *arrayInternalType << "\n";
      while (arrayInternalType->isPointerTy()) {
        /* treat pointers as their base type */
        arrayInternalType = arrayInternalType->getPointerElementType();
        outs() << "[convertAggregateTypeArgToBVTree] "
               << "argType (updated): " << *arrayInternalType << "\n";
      }

      /* since this is an arrayType, create another BVTree to hold the BVTree
       * for the arrayInternalType, for GEP indexing to work. */
      BVTree *child = new BVTree();
      parent->children.push_back(child);
      if (arrayInternalType->isStructTy()) {
        StructType *arrayInternalStructType =
            dyn_cast<StructType>(arrayInternalType);
        BVTree *grandChild = new BVTree();
        child->children.push_back(grandChild);
        if ((!arrayInternalStructType->isOpaque()) &&
            isRelevantStruct(arrayInternalStructType)) {
          lookInsideStruct(grandChild, arrayInternalStructType, prefix);
        }

      } else if (arrayInternalType->isSingleValueType()) {
        /* internal type is not a struct (i64, i32, etc.) */
        z3::expr inBV =
            BitVecHelper::getBitVec(arrayInternalType->getIntegerBitWidth(), prefix);
        outs() << "[convertAggregateTypeArgToBVTree] "
               << "arrayInternalType is singleValueType:"
               << inBV.to_string().c_str() << "\n";
        BVTree *inBVNode = new BVTree(inBV);
        child->children.push_back(inBVNode);
      }
      continue;
    }
#endif

    if (internalType->isStructTy()) { /* internal type is a struct */
      StructType *internalStructType = dyn_cast<StructType>(internalType);
      BVTree *child = new BVTree();
      parent->children.push_back(child);
      if ((!internalStructType->isOpaque()) &&
          isRelevantStruct(internalStructType)) {
        convertAggregateTypeArgToBVTree(child, internalStructType, prefix);
      }
      continue;
    }

    assert(internalType->isIntegerTy());
    if (internalType->isIntegerTy()) {
      /* internal type is not a struct (i64, i32, etc.) */
      z3::expr inBV =
          BitVecHelper::getBitVec(internalType->getIntegerBitWidth(), prefix);
      outs() << "[lookInsideStruct] "
             << "singleValueType:" << inBV.to_string().c_str() << "\n";
      BVTree *inBVNode = new BVTree(inBV);
      parent->children.push_back(inBVNode);
      continue;
    }
  }
}

#if 0
void FunctionEncoder::flattenArgToBVTree(BVTree *root, Type *argType,
                                         std::string prefix) {
  outs() << "[flattenArgToBVTree] "
         << "argType: " << *argType << "\n";
  if (argType->isPointerTy()) { /* argument is a pointer type */
    /* treat pointers as their base type */
    argType = argType->getPointerElementType();
    outs() << "[flattenArgToBVTree] "
           << "argType (updated): " << *argType << "\n";
  }
  /* Only flatten struct types */
  assert(argType->isStructTy());
  StructType *structArgType = dyn_cast<StructType>(argType);
  if ((!structArgType->isOpaque()) && isRelevantStruct(structArgType)) {
    lookInsideStructForBVTreeSetup(root, structArgType, prefix);
  }
}
#endif

BVTree *FunctionEncoder::setupBVTreeForArg(Value *argVal, std::string prefix) {
  BVTree *BVTreeForArg = new BVTree();
  Type *argType = argVal->getType();
  outs() << "[setupBVTreeForArg] "
         << "argVal: " << *argVal << "\n";
  outs() << "[setupBVTreeForArg] "
         << "argType: " << *argType << "\n";
  if (argType->isIntegerTy()) { /* argument is like i32, i64, etc. */
    outs() << "[setupBVTreeForArg] "
           << "IntegerTy "
           << "\n";
    z3::expr inBV = BitVecHelper::getBitVecSingValType(argVal);
    BVTreeForArg->bv = inBV;
  } else {
    /* argument is a struct type, array type or pointer to struct type */
    if (argType->isPointerTy()) { /* argument is a pointer type */
      /* treat pointers as their base type */
      argType = argType->getPointerElementType();
      outs() << "[setupBVTreeForArg] "
             << "argType (updated): " << *argType << "\n";
    }
    /* Only flatten struct types */
    if (!argType->isStructTy()) {
      throw std::invalid_argument(
          "[setupBVTreeForArg] argument is not a struct type\n");
    }
    StructType *structArgType = dyn_cast<StructType>(argType);
    if ((!structArgType->isOpaque()) && isRelevantStruct(structArgType)) {
      convertAggregateTypeArgToBVTree(BVTreeForArg, structArgType, prefix);
    }
  }
  outs() << "[setupBVTreeForArg] "
         << "returning BVTree: " << BVTreeForArg->toString() << "\n";
  return BVTreeForArg;
}

/* Main encoding loop: get an encoding of the llvm:Function F's body. Creates
 * a new vector of expressions (z3::expr_vector) and populates the vector with
 * the encoding of F. */
z3::expr_vector FunctionEncoder::encodeFunctionBody(Function &F) {

  /* First pass: populate BBAssertionsMap */
  outs()
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n"
      << "Pass #1: populateBBAssertionsMap\n"
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n";

  for (BasicBlock &BB : F) {
    /* Update current BasicBlock */
    currentBB = &BB;
    /* Make entry for BasicBlock in BBAssertionsMap if it doesn't exist*/
    z3::expr_vector BBAsstVec(ctx);
    if (BBAssertionsMap.find(currentBB) == BBAssertionsMap.end()) {
      BBAssertionsMap.insert({currentBB, BBAsstVec});
    }

    outs() << "=========================\n";
    outs() << currentBB->getName() << "\n";
    outs() << "=========================\n";

    /* check if current BB has a MemoryPhi associated with it*/
    MemoryPhi *memoryPhiCurrentBB =
        currentMemorySSA->getMemoryAccess(currentBB);
    if (memoryPhiCurrentBB) {
      FunctionEncoder::handleMemoryPhiNode(*memoryPhiCurrentBB, 1);
    }

    populateBBAssertionsMap(BB);
  }

  /* Second pass: populate pathConditionsMap */
  outs()
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n"
      << "Pass #2: populatePathConditionsMap\n"
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n";

  for (BasicBlock &BB : F) {
    // Update current BasicBlock
    currentBB = &BB;
    outs() << "================\n";
    outs() << currentBB->getName() << "\n";
    outs() << "================\n";

    /* Nothing really happens to memoryPhiNodes in this pass. This is just a
     * placeholder to maintain the invariant that whenever we are iterating a
     * basicblock, the mostRecentMemoryDef stays updated. */
    MemoryPhi *memoryPhiCurrentBB =
        currentMemorySSA->getMemoryAccess(currentBB);
    if (memoryPhiCurrentBB) {
      FunctionEncoder::handleMemoryPhiNode(*memoryPhiCurrentBB, 2);
    }
    populatePathConditionsMap(BB);
  }

  /* Third pass: FunctionEncoder::handlePhiNodes (including MemoryPhi) */
  outs()
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n"
      << "Pass #3: FunctionEncoder::handlePhiNodes\n"
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n";

  for (BasicBlock &BB : F) {
    currentBB = &BB; // update current BB

    outs() << "================\n";
    outs() << currentBB->getName() << "\n";
    outs() << "================\n";

    /* check if current BB has a MemoryPhi associated with it */
    MemoryPhi *memoryPhiCurrentBB = currentMemorySSA->getMemoryAccess(&BB);
    if (memoryPhiCurrentBB) {
      FunctionEncoder::handleMemoryPhiNode(*memoryPhiCurrentBB, 3);
    }

    for (Instruction &I : BB) {
      if (isa<PHINode>(&I)) {
        outs() << "-------------------\n"
               << I << "\n"
               << "-------------------\n";
        FunctionEncoder::handlePhiNode(*(dyn_cast<PHINode>(&I)), 3);
      }
    }
  }

  outs()
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n"
      << "Pass #4: FunctionEncoder::handleReturnInst\n"
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n";

  for (BasicBlock &BB : F) {
    currentBB = &BB; /* update current BB */
    MemoryPhi *memoryPhiCurrentBB = currentMemorySSA->getMemoryAccess(&BB);
    if (memoryPhiCurrentBB) {
      mostRecentMemoryDef = memoryPhiCurrentBB;
    }
    for (Instruction &I : BB) {
      if (isa<StoreInst>(&I)) {
        mostRecentMemoryDef = currentMemorySSA->getMemoryAccess(&I);
        continue;
      }
      if (isa<ReturnInst>(&I)) {
        FunctionEncoder::handleReturnInst(*(dyn_cast<ReturnInst>(&I)));
      }
    }
  }

  /* Print all global maps */
  outs() << "[encodeFunctionBody] "
         << "BBAssertionsMap:\n";
  printBBAssertionsMap();
  outs() << "[encodeFunctionBody] "
         << "PathConditionsMap:\n";
  printPathConditionsMap();
  outs() << "[encodeFunctionBody] "
         << "EdgeAssertionsMap:\n";
  printEdgeAssertionsMap();
  outs() << "[encodeFunctionBody] "
         << "MemoryAccessValueBVTreeMap:\n";
  printMemoryAccessValueBVTreeMap();

  /* Create final function formula */
  outs()
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n"
      << "Creating formula...\n"
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n";

  z3::expr_vector formula(ctx);
  for (auto KeyValue : BBAssertionsMap) {
    BasicBlock *BB = KeyValue.first;
    z3::expr_vector Z3ExprVec = KeyValue.second;
    /* If the assertions list is empty, likely the bb was composed of just one
     * unconditional jump. Nothing to encode. */
    if (Z3ExprVec.empty()) {
      continue;
    }
    bool pathCondExistsForBB =
        (pathConditionsMap.find(BB) != pathConditionsMap.end());
    if (!pathCondExistsForBB) {
      /* If there are no path conditions for this BB, directly add the list of
       * assertions associated with this BB to the solver (no implications) */
      for (z3::expr assertionExprs : Z3ExprVec) {
        formula.push_back(assertionExprs);
      }
    } else {
      /* If there are path conditions associated with this BasicBlock, add
       * (pathCond) ==> And(assertions)) */
      auto pathCond = pathConditionsMap.at(BB);
      formula.push_back(z3::implies(pathCond, z3::mk_and(Z3ExprVec)));
    }
  }

  return formula;
}

void FunctionEncoder::JsonRecursive(Json::Value *jsonRoot, BVTree *bvTreeRoot,
                                    StructType *baseStructType,
                                    int recursionDepth) {
  outs() << "[JsonRecursive] "
         << "jsonRoot: " << jsonRoot->toStyledString() << "\n";
  outs() << "[JsonRecursive] "
         << "bvTreeRoot: " << bvTreeRoot->toString() << "\n";
  outs() << "[JsonRecursive] "
         << "baseStructType: " << baseStructType->getName() << "\n";
  outs() << "[JsonRecursive] "
         << "recursionDepth: " << recursionDepth << "\n";

  int i = 0;
  for (auto internalTypeIterator = baseStructType->subtype_begin();
       internalTypeIterator != baseStructType->subtype_end();
       internalTypeIterator++) {
    Type *internalType = *internalTypeIterator;
    outs() << "[JsonRecursive] "
           << "internalType: " << *internalType << "\n";

    while (internalType->isPointerTy()) {
      /* treat pointers as their base type */
      internalType = internalType->getPointerElementType();
      outs() << "[JsonRecursive] "
             << "internalType (updated): " << *internalType << "\n";
    }

    if (internalType == baseStructType) { /* this is a linked list pointer */
      jsonRoot->append("");
      outs() << "[JsonRecursive] "
             << "pointer to same type (i.e. linked list), continuing\n";
      continue;
    }

    assert(!internalType->isArrayTy());

    if (internalType->isStructTy()) { /* internal type is a struct */
      StructType *internalStructType = dyn_cast<StructType>(internalType);
      outs() << "[JsonRecursive] "
             << "internalStructType:" << *internalStructType << "\n";
      Json::Value *childJsonValue = new Json::Value(Json::arrayValue);
      if (internalStructType->isOpaque() ||
          (!isRelevantStruct(internalStructType))) {
        outs() << "[JsonRecursive] "
               << "opaque or not relevant\n";
        childJsonValue->append("");
        outs() << "[JsonRecursive] "
               << "childJsonValue:" << childJsonValue->toStyledString() << "\n";
      } else {
        outs() << "[JsonRecursive] "
               << "relevant\n";
        BVTree *childBVTree = bvTreeRoot->getSubTree(i);
        outs() << "[JsonRecursive] "
               << "childBVTree:" << childBVTree->toString() << "\n";
        JsonRecursive(childJsonValue, childBVTree, internalStructType,
                      recursionDepth + 1);
      }
      jsonRoot->append(*childJsonValue);
      outs() << "[JsonRecursive] "
             << "jsonRoot:" << jsonRoot->toStyledString() << "\n";
    } else if (internalType->isIntegerTy()) {
      IntegerType *integerType = dyn_cast<IntegerType>(internalType);
      outs() << "[JsonRecursive] "
             << "isIntegerTy:" << *integerType << "\n";
      /* internal type is not a struct (i64, i32, etc.) */
      BVTree *internalBVTree = bvTreeRoot->getSubTree(i);
      z3::expr bv = internalBVTree->bv;
      outs() << "[JsonRecursive] "
             << "internalBVTree:" << internalBVTree->toString() << "\n";

      auto jsonVal = internalBVTree->bv.to_string() == "null"
                         ? ""
                         : internalBVTree->bv.to_string();
      jsonRoot->append(jsonVal);
      outs() << "[JsonRecursive] "
             << "jsonRoot:" << jsonRoot->toStyledString() << "\n";
    } else {
      throw std::runtime_error("[JsonRecursive] internalType is not a "
                               "struct type or integer type\n");
    }
    i++;
  }
}

/* https://en.wikibooks.org/wiki/JsonCpp */
Json::Value *FunctionEncoder::getJsonDictFromValueBVTree(Value *val,
                                                         BVTree *bvT) {

  Json::Value *jsonRoot = new Json::Value(Json::arrayValue);
  Type *type = val->getType();
  outs() << "[getJsonDictFromValueBVTree] "
         << "type: " << *type << "\n";
  if (type->isIntegerTy()) { /* argument is like i32, i64, etc. */
    outs() << "[getJsonDictFromValueBVTree] "
           << "IntegerTy "
           << "\n";
    jsonRoot->append(bvT->bv.to_string());
    outs() << "[getJsonDictFromValueBVTree] "
           << "jsonRoot:" << jsonRoot->toStyledString() << "\n";
  } else {
    if (type->isPointerTy()) {
      type = type->getPointerElementType();
      outs() << "[getJsonDictFromValueBVTree] "
             << "type (updated): " << *type << "\n";
    }
    if (!type->isStructTy()) {
      throw std::invalid_argument(
          "[getJsonDictFromValueBVTree] not a struct type\n");
    }
    StructType *structType = dyn_cast<StructType>(type);
    if (structType->isOpaque() || (!isRelevantStruct(structType))) {
      jsonRoot->append("");
    } else {
      JsonRecursive(jsonRoot, bvT, structType, 0);
    }
  }
  return jsonRoot;
}

void FunctionEncoder::populateInputAndOutputJsonDict() {

  this->inputJsonDict = new Json::Value(Json::objectValue);
  this->outputJsonDict = new Json::Value(Json::objectValue);

  outs() << "[populateInputAndOutputJsonDict] "
         << "Populating input json dict\n";

  /* Populate input json dict for from inputValueBVTreeMap only considering
   * those Values which were function arguments */
  for (auto argIterator = currentFunction->arg_begin();
       argIterator != currentFunction->arg_end(); argIterator++) {
    Value *argVal = dyn_cast<Value>(argIterator);
    outs() << "[populateInputAndOutputJsonDict] "
           << "argVal: " << *argVal << "\n";

    BVTree *argValInputBVTree = this->inputValueBVTreeMap->find(argVal)->second;
    outs() << "[populateInputAndOutputJsonDict] "
           << "argValInputBVTree: " << argValInputBVTree->toString() << "\n";
    Json::Value *argValJsonInput =
        getJsonDictFromValueBVTree(argVal, argValInputBVTree);
    (*this->inputJsonDict)[argVal->getName().str()] = *argValJsonInput;
  }
  outs() << "[populateInputAndOutputJsonDict] "
         << "Populating output json dict\n";

  /* Populate output json dict directly from outputValueBVTreeMap */
  for (auto ValueBVTreePair : *this->outputValueBVTreeMap) {
    Value *outputVal = ValueBVTreePair.first;
    BVTree *outputBVTree = ValueBVTreePair.second;
    outs() << "[populateInputAndOutputJsonDict] "
           << "outputBVTree: " << outputBVTree->toString() << "\n";
    Json::Value *argValJsonOutput =
        getJsonDictFromValueBVTree(outputVal, outputBVTree);
    (*this->outputJsonDict)[outputVal->getName().str()] = *argValJsonOutput;
  }
}

/* This method implements what the pass does */
void FunctionEncoder::buildSMT() {

  this->mostRecentMemoryDef = this->currentMemorySSA->getLiveOnEntryDef();

  outs()
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n"
      << "Setup input BVTrees \n"
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n";

  this->MemoryAccessValueBVTreeMap.insert(
      {this->mostRecentMemoryDef, *this->inputValueBVTreeMap});
  this->inputValueBVTreeMap =
      &this->MemoryAccessValueBVTreeMap.find(this->mostRecentMemoryDef)->second;
  this->currentBB = &this->currentFunction->getEntryBlock();
  outs() << "[buildSMT] "
         << "currentBB:" << this->currentBB->getName() << "\n";

  /* Create the first entry in BBAssertionsMap for entry BB */
  z3::expr_vector entryBBAsstVec(ctx);
  BBAssertionsMap.insert({this->currentBB, entryBBAsstVec});

  /* Iterate over all arguments of function F and build a
   * MemoryAccessValueBVTreeMap. The only key will be 0 (liveOnEntry). The
   * value in the map will be a vector of BVTrees corresponding to each input
   * argument to this function. */
  for (auto argIterator = this->currentFunction->arg_begin();
       argIterator != this->currentFunction->arg_end(); argIterator++) {
    Value *argVal = dyn_cast<Value>(argIterator);
    BVTree *BVTreeForArg =
        this->setupBVTreeForArg(argVal, argVal->getName().str());
    this->inputValueBVTreeMap->insert({argVal, BVTreeForArg});

    /* For each bitvector b in the BVTree, assert that (b == b) in entryBB's
     * asertions in BBAssertionsMap. This is mostly superflous, but is
     * necessary to have the output SMT contain the bitvectors corresponding
     * to the functions's input. It is particularly necessary when the
     * function has just one basic block (no phi block to merge two blocks
     * using new bitvectors)  */
    BVTreeForArg->addBVSelfEquivalencesToExprVec(entryBBAsstVec);

    /* Add arg to FunctionArgs list */
    this->FunctionArgs.insert(argVal);
  }
  printMemoryAccessValueBVTreeMap();

  if (this->currentFunction->getReturnType()->isVoidTy()) {
    this->functionReturnsVoid = true;
  }

  /* Call encodeFunctionBody to get encoding for function.  */
  this->functionEncodingZ3ExprVec =
      this->encodeFunctionBody(*this->currentFunction);

  outs()
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n"
      << "Finalizing JSON input and output dictionaries...\n"
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n";
  this->populateInputAndOutputJsonDict();

  outs()
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n"
      << "Printing Encoding...\n"
      << "<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>\n";
  outs() << this->toString().c_str();
}