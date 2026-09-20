// Copyright 2026 keplertech.io
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <sstream>

#include "export/SecBtor2Exporter.h"

#include "BoolExprCache.h"
#include "model/SequentialDesignModel.h"
#include "proof/InternalRelations.h"
#include "strategy/SequentialEquivalenceStrategy.h"

namespace KEPLER_FORMAL::SEC {
namespace {

class InternalRelationsTests : public ::testing::Test {
 protected:
  void TearDown() override { BoolExprCache::destroy(); }

  KInductionProblem binaryRing() {
    KInductionProblem problem;
    problem.allSymbols = {2, 3, 4, 5};
    problem.state0Symbols = {2, 3};
    problem.state1Symbols = {4, 5};
    problem.initialStateAssignments = {{2, false}, {3, false}, {4, false}, {5, false}};
    problem.transitions0 = {{2, BoolExpr::Var(3)}, {3, BoolExpr::Not(BoolExpr::Var(2))}};
    problem.transitions1 = {{4, BoolExpr::Var(5)}, {5, BoolExpr::Not(BoolExpr::Var(4))}};
    return problem;
  }

  KInductionProblem heldX() {
    auto problem = binaryRing();
    problem.usesDualRailStateEncoding = true;
    problem.dualRailStatePairs = {{2, 3}, {4, 5}};
    problem.initialStateAssignments = {{2, true}, {3, true}, {4, true}, {5, true}};
    problem.transitions0 = {{2, BoolExpr::Var(2)}, {3, BoolExpr::Var(3)}};
    problem.transitions1 = {{4, BoolExpr::Var(4)}, {5, BoolExpr::Var(5)}};
    return problem;
  }

  SequentialDesignModel heldModel(bool initialized, bool value, bool extraKnownOutput = false) {
    const SignalKey state{{1}, {1}};
    const SignalKey output{{2}, {2}};
    SequentialDesignModel model;
    model.stateBits = {state};
    model.inputVarByKey[state] = 2;
    model.displayNameByKey[state] = "q";
    model.nextStateExprByStateKey[state] = BoolExpr::Var(2);
    model.allObservedOutputs = model.observedOutputs = {output};
    model.displayNameByKey[output] = "y";
    model.observedOutputExprByKey[output] = BoolExpr::Var(2);
    if (initialized) {
      model.initialStateValueByKey[state] = value;
    }
    if (extraKnownOutput) {
      const SignalKey known{{3}, {3}};
      model.allObservedOutputs.push_back(known);
      model.observedOutputs.push_back(known);
      model.displayNameByKey[known] = "known";
      model.observedOutputExprByKey[known] = BoolExpr::createTrue();
    }
    return model;
  }
};

TEST_F(InternalRelationsTests, JointInductionProvesMutuallySupportingRegisters) {
  const auto problem = binaryRing();
  const InternalRelationCandidate first{{{2, 4}}, {}};
  const InternalRelationCandidate second{{{3, 5}}, {}};
  for (auto solver : {Config::SolverType::KISSAT, Config::SolverType::CADICAL,
                      Config::SolverType::GLUCOSE}) {
    EXPECT_TRUE(proveInternalRelations(problem, {first}, {}, solver).empty());
    EXPECT_EQ(proveInternalRelations(problem, {first, second}, {}, solver).size(), 2u);
    EXPECT_TRUE(proveInternalRelations(problem, {first, second}, {false, true}, solver).empty());
  }
}

TEST_F(InternalRelationsTests, RefutedHypothesesAreRemovedBeforeRecheckingDependents) {
  auto problem = binaryRing();
  problem.transitions1[1].second = BoolExpr::Var(4);
  EXPECT_TRUE(proveInternalRelations(problem, {{{{2, 4}}, {}}, {{{3, 5}}, {}}}, {},
                                    Config::SolverType::KISSAT).empty());
}

TEST_F(InternalRelationsTests, IndependentUninitializedBooleanRegistersAreNotEqual) {
  auto problem = binaryRing();
  problem.initialStateAssignments.clear();
  EXPECT_TRUE(proveInternalRelations(problem, {{{{2, 4}}, {}}, {{{3, 5}}, {}}}, {},
                                    Config::SolverType::KISSAT).empty());
}

TEST_F(InternalRelationsTests, XEqualityUsesCompleteTernaryValuesAndHonorsSwitch) {
  auto problem = heldX();
  const InternalRelationCandidate candidate{{{2, 4}, {3, 5}}, {{2, 3}, {4, 5}}};
  EXPECT_EQ(proveInternalRelations(problem, {candidate}, {}, Config::SolverType::KISSAT).size(), 2u);
  EXPECT_TRUE(proveInternalRelations(problem, {candidate}, {true, false},
                                    Config::SolverType::KISSAT).empty());
  problem.initialStateAssignments[3].second = false;
  EXPECT_TRUE(proveInternalRelations(problem, {candidate}, {}, Config::SolverType::KISSAT).empty());
}

TEST_F(InternalRelationsTests, MatchingXAtBootDoesNotCertifyDivergingTransitions) {
  auto problem = heldX();
  problem.transitions0 = {{2, BoolExpr::createTrue()}, {3, BoolExpr::createFalse()}};
  problem.transitions1 = {{4, BoolExpr::createFalse()}, {5, BoolExpr::createTrue()}};
  EXPECT_TRUE(proveInternalRelations(problem, {{{{2, 4}, {3, 5}}, {{2, 3}, {4, 5}}}}, {},
                                    Config::SolverType::KISSAT).empty());
}

TEST_F(InternalRelationsTests, KnownValuesCanBeLearnedWithXEqualityDisabled) {
  auto problem = heldX();
  problem.initialStateAssignments = {{2, false}, {3, true}, {4, false}, {5, true}};
  EXPECT_EQ(proveInternalRelations(problem, {{{{2, 4}, {3, 5}}, {{2, 3}, {4, 5}}}},
                                  {true, false}, Config::SolverType::KISSAT).size(), 2u);
}

TEST_F(InternalRelationsTests, DisallowingXChecksDefinednessInTheInductionStepToo) {
  auto problem = heldX();
  problem.initialStateAssignments = {{2, false}, {3, true}, {4, false}, {5, true}};
  problem.transitions0 = {{2, BoolExpr::createTrue()}, {3, BoolExpr::createTrue()}};
  problem.transitions1 = {{4, BoolExpr::createTrue()}, {5, BoolExpr::createTrue()}};
  const InternalRelationCandidate candidate{{{2, 4}, {3, 5}}, {{2, 3}, {4, 5}}};
  EXPECT_EQ(proveInternalRelations(problem, {candidate}, {}, Config::SolverType::KISSAT).size(), 2u);
  EXPECT_TRUE(proveInternalRelations(problem, {candidate}, {true, false},
                                    Config::SolverType::KISSAT).empty());
}

TEST_F(InternalRelationsTests, OptionsLeaveTheOutputProofObligationUnchanged) {
  const auto model = heldModel(false, false);
  for (bool learn : {false, true}) {
    for (bool allowX : {false, true}) {
      auto problem = heldX();
      problem.property = BoolExpr::createFalse();
      problem.bad = BoolExpr::createTrue();
      problem.inductionProperty = BoolExpr::Var(2);
      problem.observedOutputExprs0 = {BoolExpr::Var(2)};
      problem.observedOutputExprs1 = {BoolExpr::Var(4)};
      learnInternalStateRelations(model, model, problem, {learn, allowX},
                                  Config::SolverType::KISSAT, false);
      EXPECT_EQ(problem.property, BoolExpr::createFalse());
      EXPECT_EQ(problem.bad, BoolExpr::createTrue());
      EXPECT_EQ(problem.inductionProperty, BoolExpr::Var(2));
      EXPECT_EQ(problem.observedOutputExprs0, std::vector<BoolExpr*>({BoolExpr::Var(2)}));
      EXPECT_EQ(problem.observedOutputExprs1, std::vector<BoolExpr*>({BoolExpr::Var(4)}));
      EXPECT_EQ(problem.sameFrameStateEqualityPairs0.size(), learn && allowX ? 2u : 0u);
    }
  }
}

TEST_F(InternalRelationsTests, DisablingBothSwitchesPreservesThePreparedProblemExactly) {
  const auto model = heldModel(false, false);
  auto problem = heldX();
  problem.property = BoolExpr::Not(BoolExpr::Xor(BoolExpr::Var(2), BoolExpr::Var(4)));
  problem.bad = BoolExpr::Not(problem.property);
  problem.observedOutputNames = {"y"};
  problem.observedOutputExprs0 = {BoolExpr::Var(2)};
  problem.observedOutputExprs1 = {BoolExpr::Var(4)};
  std::ostringstream before, after;
  exportSecBtor2(problem, before);
  learnInternalStateRelations(model, model, problem, {false, false},
                              Config::SolverType::KISSAT, false);
  exportSecBtor2(problem, after);
  EXPECT_EQ(before.str(), after.str());
  EXPECT_TRUE(problem.sameFrameStateEqualityPairs0.empty());
}

TEST_F(InternalRelationsTests, OutputAssumptionsCannotCertifyAnInvalidInternalRelation) {
  auto problem = heldX();
  problem.property = problem.inductionProperty = BoolExpr::createFalse();
  problem.transitions0 = {{2, BoolExpr::createTrue()}, {3, BoolExpr::createFalse()}};
  problem.transitions1 = {{4, BoolExpr::createFalse()}, {5, BoolExpr::createTrue()}};
  const auto model = heldModel(false, false);
  learnInternalStateRelations(model, model, problem, {}, Config::SolverType::KISSAT, false);
  EXPECT_TRUE(problem.sameFrameStateEqualityPairs0.empty());
}

TEST_F(InternalRelationsTests, SharedDriversSuggestOnlyInitiallyCompatibleSameDesignRelations) {
  auto model = heldModel(true, false);
  const SignalKey second{{5}, {5}};
  model.stateBits.push_back(second);
  model.inputVarByKey[second] = 3;
  model.displayNameByKey[second] = "other_name";
  for (const auto& key : model.stateBits) {
    model.nextStateExprByStateKey[key] = BoolExpr::createFalse();
  }
  auto problem = binaryRing();
  problem.transitions0 = {{2, BoolExpr::createFalse()}, {3, BoolExpr::createFalse()}};
  problem.state1Symbols.clear();
  problem.transitions1.clear();
  problem.allSymbols = {2, 3};
  problem.initialStateAssignments = {{2, false}, {3, false}};
  learnInternalStateRelations(model, {}, problem, {}, Config::SolverType::KISSAT, false);
  EXPECT_EQ(problem.sameFrameStateEqualityPairs0,
            (std::vector<std::pair<size_t, size_t>>{{2, 3}}));
  problem.sameFrameStateEqualityPairs0.clear();
  problem.initialStateAssignments[1].second = true;
  learnInternalStateRelations(model, {}, problem, {}, Config::SolverType::KISSAT, false);
  EXPECT_TRUE(problem.sameFrameStateEqualityPairs0.empty());
}

TEST_F(InternalRelationsTests, SameNamesAndMatchingStartupXCannotHideBinaryMismatch) {
  EXPECT_TRUE(InternalRelationOptions{}.learnInternalRelations);
  EXPECT_TRUE(InternalRelationOptions{}.allowXEqualityInInternalRelations);
  for (bool initialized : {false, true}) {
    auto zero = heldModel(initialized, false);
    zero.nextStateExprByStateKey.begin()->second = BoolExpr::createFalse();
    auto diverging = zero;
    diverging.nextStateExprByStateKey.begin()->second = BoolExpr::createTrue();
    for (auto engine : {SecEngine::Pdr, SecEngine::KInduction, SecEngine::Imc}) {
      for (bool learn : {false, true}) {
        for (bool allowX : {false, true}) {
          SCOPED_TRACE(::testing::Message() << "engine=" << static_cast<int>(engine)
              << " initialized=" << initialized << " learn=" << learn << " allowX=" << allowX);
          SequentialEquivalenceStrategy strategy(nullptr, nullptr, Config::SolverType::KISSAT, engine);
          strategy.setInternalRelationOptions({learn, allowX});
          EXPECT_EQ(strategy.runExtractedModels(zero, diverging, 2).status,
                    SequentialEquivalenceStatus::Different);
          if (initialized) {
            EXPECT_EQ(strategy.runExtractedModels(zero, zero, 2).status,
                      SequentialEquivalenceStatus::Equivalent);
          }
        }
      }
    }
  }
}

TEST_F(InternalRelationsTests, ResetProofBehaviorIsPreservedWithLearning) {
  auto model = heldModel(false, false);
  const SignalKey reset{{4}, {4}};
  model.environmentInputs = {reset};
  model.inputVarByKey[reset] = 3;
  model.displayNameByKey[reset] = "rst";
  model.nextStateExprByStateKey.begin()->second = BoolExpr::And(
      BoolExpr::Not(BoolExpr::Var(3)), BoolExpr::Var(2));
  for (auto engine : {SecEngine::Pdr, SecEngine::KInduction, SecEngine::Imc}) {
    SequentialEquivalenceStrategy strategy(nullptr, nullptr, Config::SolverType::KISSAT,
        engine, SecEncoding::DualRailSteady, {1, {{"rst", true}}});
    strategy.setInternalRelationOptions({false, false});
    const auto baseline = strategy.runExtractedModels(model, model, 2);
    for (bool allowX : {false, true}) {
      strategy.setInternalRelationOptions({true, allowX});
      const auto result = strategy.runExtractedModels(model, model, 2);
      EXPECT_EQ(result.status, baseline.status);
      EXPECT_EQ(result.coveredOutputs, baseline.coveredOutputs);
    }
  }
}

TEST_F(InternalRelationsTests, UnfinishedRefinementPublishesNoSurvivingGuesses) {
  KInductionProblem problem;
  std::vector<InternalRelationCandidate> candidates;
  for (size_t i = 0; i < 70; ++i) {
    const size_t lhs = 2 + 2 * i;
    const size_t rhs = lhs + 1;
    problem.allSymbols.insert(problem.allSymbols.end(), {lhs, rhs});
    problem.state0Symbols.push_back(lhs);
    problem.state1Symbols.push_back(rhs);
    problem.initialStateAssignments.emplace_back(lhs, false);
    problem.initialStateAssignments.emplace_back(rhs, false);
    problem.transitions0.emplace_back(lhs,
        i == 69 ? BoolExpr::createTrue() : BoolExpr::Var(lhs + 2));
    problem.transitions1.emplace_back(rhs,
        i == 69 ? BoolExpr::createFalse() : BoolExpr::Var(rhs + 2));
    candidates.push_back({{{lhs, rhs}}, {}});
  }
  EXPECT_TRUE(proveInternalRelations(problem, candidates, {}, Config::SolverType::KISSAT).empty());
}

}  // namespace
}  // namespace KEPLER_FORMAL::SEC
