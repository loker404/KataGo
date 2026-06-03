#include "../game/graphhash.h"
#include "../core/test.h"

static const Hash128 FLYING_KNIFE_RECONSTRUCTION_FALLBACK_HASH = Hash128(0xf0cae71c9a22b55dULL,0x5f674c38406b8e19ULL);

static bool shouldApplyFlyingKnifeChanceTransition(const BoardHistory& hist) {
  const FlyingKnifeConfig& fkConfig = hist.rules.fkConfig;
  if(!fkConfig.isEnabled())
    return false;
  if(hist.fkState.isInSequence())
    return false;

  int moveNumber = (int)hist.moveHistory.size();
  if(moveNumber <= 0)
    return false;
  if(hist.moveHistory[moveNumber-1].loc == Board::PASS_LOC)
    return false;
  if(moveNumber < fkConfig.triggerRangeStart || moveNumber > fkConfig.triggerRangeEnd)
    return false;

  Player pla = hist.moveHistory[moveNumber-1].pla;
  return hist.fkState.getKnivesRemaining(pla) + hist.fkState.getSicklesRemaining(pla) > 0;
}

static int getRecordedFlyingKnifeAbilityMoves(const BoardHistory& hist, int turnIdx) {
  if(hist.flyingKnifeTriggerHistory.size() != hist.moveHistory.size())
    return -1;
  testAssert(turnIdx >= 0 && (size_t)turnIdx < hist.flyingKnifeTriggerHistory.size());
  return hist.flyingKnifeTriggerHistory[turnIdx];
}

static int inferFlyingKnifeAbilityMoves(
  const BoardHistory& histOrig,
  const BoardHistory& hist,
  int turnIdx
) {
  const std::vector<Move>& moves = histOrig.moveHistory;
  Player pla = moves[turnIdx].pla;

  int extraMoves = 0;
  for(int i = turnIdx+1; i<moves.size() && moves[i].pla == pla; i++)
    extraMoves++;

  int remainingMovesAtEnd = 0;
  if(turnIdx + 1 + extraMoves >= moves.size() &&
     histOrig.fkState.isInSequence() &&
     histOrig.fkState.abilityOwner == pla)
    remainingMovesAtEnd = histOrig.fkState.remainingMovesInSequence;

  int knives = hist.fkState.getKnivesRemaining(pla);
  int sickles = hist.fkState.getSicklesRemaining(pla);
  return FlyingKnifeConfig::inferUniqueAbilityMovesFromRun(extraMoves, remainingMovesAtEnd, knives, sickles);
}

static bool maybeApplyInferredFlyingKnifeChanceTrigger(
  const BoardHistory& histOrig,
  int turnIdx,
  const Board& board,
  BoardHistory& hist
) {
  if(!shouldApplyFlyingKnifeChanceTransition(hist))
    return false;

  Player pla = hist.moveHistory[hist.moveHistory.size()-1].pla;
  int recordedAbilityMoves = getRecordedFlyingKnifeAbilityMoves(histOrig, turnIdx);
  int abilityMoves = recordedAbilityMoves >= 0 ? recordedAbilityMoves : inferFlyingKnifeAbilityMoves(histOrig, hist, turnIdx);
  if(abilityMoves <= 0)
    return false;

  return hist.applyFlyingKnifeAbility(board, (int)hist.moveHistory.size(), pla, abilityMoves);
}

Hash128 GraphHash::getStateHash(const BoardHistory& hist, Player nextPlayer, double drawEquivalentWinsForWhite) {
  const Board& board = hist.getRecentBoard(0);
  Hash128 hash = BoardHistory::getSituationRulesAndKoHash(board, hist, nextPlayer, drawEquivalentWinsForWhite);

  // Fold in whether a pass ends this phase
  bool passEndsPhase = hist.passWouldEndPhase(board,nextPlayer);
  if(passEndsPhase)
    hash ^= Board::ZOBRIST_PASS_ENDS_PHASE;
  // Fold in whether the game is over or not
  if(hist.isGameFinished)
    hash ^= Board::ZOBRIST_GAME_IS_OVER;

  // Fold in consecutive pass count. Probably usually redundant with history tracking. Use some standard LCG constants.
  static constexpr uint64_t CONSECPASS_MULT0 = 2862933555777941757ULL;
  static constexpr uint64_t CONSECPASS_MULT1 = 3202034522624059733ULL;
  hash.hash0 += CONSECPASS_MULT0 * (uint64_t)hist.consecutiveEndingPasses;
  hash.hash1 += CONSECPASS_MULT1 * (uint64_t)hist.consecutiveEndingPasses;

  BoardHistory::mixFlyingKnifeStateHash(hist.rules,hist.fkState,hash);

  return hash;
}

Hash128 GraphHash::getGraphHash(Hash128 prevGraphHash, const BoardHistory& hist, Player nextPlayer, int repBound, double drawEquivalentWinsForWhite) {
  const Board& board = hist.getRecentBoard(0);
  Loc prevMoveLoc = hist.moveHistory.size() <= 0 ? Board::NULL_LOC : hist.moveHistory[hist.moveHistory.size()-1].loc;
  if(prevMoveLoc == Board::NULL_LOC || board.simpleRepetitionBoundGt(prevMoveLoc,repBound)) {
    return getStateHash(hist,nextPlayer,drawEquivalentWinsForWhite);
  }
  else {
    Hash128 newHash = prevGraphHash;
    newHash.hash0 = Hash::splitMix64(newHash.hash0 ^ newHash.hash1);
    newHash.hash1 = Hash::nasam(newHash.hash1) + newHash.hash0;
    Hash128 stateHash = getStateHash(hist,nextPlayer,drawEquivalentWinsForWhite);
    newHash.hash0 += stateHash.hash0;
    newHash.hash1 += stateHash.hash1;
    return newHash;
  }
}

Hash128 GraphHash::getGraphHashFromScratch(const BoardHistory& histOrig, Player nextPlayer, int repBound, double drawEquivalentWinsForWhite) {
  BoardHistory hist = histOrig.copyToInitial();
  Board board = hist.getRecentBoard(0);
  Hash128 graphHash = Hash128();

  Player initialNextPlayer = histOrig.moveHistory.size() <= 0 ? nextPlayer : histOrig.moveHistory[0].pla;
  graphHash = getGraphHash(graphHash, hist, initialNextPlayer, repBound, drawEquivalentWinsForWhite);

  for(size_t i = 0; i<histOrig.moveHistory.size(); i++) {
    bool suc = hist.makeBoardMoveTolerant(board, histOrig.moveHistory[i].loc, histOrig.moveHistory[i].pla, histOrig.preventEncoreHistory[i]);
    testAssert(suc);
    Player actualNextPlayer = i+1 < histOrig.moveHistory.size() ? histOrig.moveHistory[i+1].pla : nextPlayer;
    bool hasChanceTransition = shouldApplyFlyingKnifeChanceTransition(hist);
    graphHash = getGraphHash(
      graphHash, hist, hasChanceTransition ? hist.presumedNextMovePla : actualNextPlayer,
      repBound, drawEquivalentWinsForWhite
    );
    if(hasChanceTransition) {
      maybeApplyInferredFlyingKnifeChanceTrigger(histOrig, (int)i, board, hist);
      graphHash = getGraphHash(graphHash, hist, hist.fkState.isInSequence() ? hist.presumedNextMovePla : actualNextPlayer, repBound, drawEquivalentWinsForWhite);
    }
  }

  if(hist.fkState != histOrig.fkState || hist.presumedNextMovePla != nextPlayer) {
    // Histories with externally forced FK state, or ambiguous consecutive-run
    // decompositions, cannot always be uniquely reconstructed from moveHistory
    // alone. In those cases, mix in the exact current state while preserving
    // the replayed graph hash's path dependence.
    hist.fkState = histOrig.fkState;
    hist.presumedNextMovePla = nextPlayer;
    hist.recomputeCurrentKoHash(board);
    graphHash.hash0 = Hash::splitMix64(graphHash.hash0 ^ graphHash.hash1 ^ FLYING_KNIFE_RECONSTRUCTION_FALLBACK_HASH.hash0);
    graphHash.hash1 = Hash::nasam(graphHash.hash1 ^ FLYING_KNIFE_RECONSTRUCTION_FALLBACK_HASH.hash1) + graphHash.hash0;
    Hash128 stateHash = getStateHash(hist, nextPlayer, drawEquivalentWinsForWhite);
    graphHash.hash0 += stateHash.hash0;
    graphHash.hash1 += stateHash.hash1;
  }
  testAssert(
    getStateHash(hist, nextPlayer, drawEquivalentWinsForWhite) ==
    getStateHash(histOrig, nextPlayer, drawEquivalentWinsForWhite)
  );

  return graphHash;
}

