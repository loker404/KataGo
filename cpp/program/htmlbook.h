#ifndef PROGRAM_HTMLBOOK_H_
#define PROGRAM_HTMLBOOK_H_

#include "../core/global.h"
#include "../core/fileutils.h"
#include "../game/board.h"
#include "../game/boardhistory.h"
#include "../search/search.h"

struct HtmlBookNode {
  int bSizeX = 0;
  int bSizeY = 0;
  int nextPla = 0;
  std::map<int,std::string> links;
  std::map<int,int> linkSyms;
  bool hasPass = false;
  double passWinpct = -1e100;
  std::map<int,double> winpctByPos;
};

class HtmlBookNavigator {
 public:
  HtmlBookNavigator(const std::string& dir, double minRelWinPct);
  bool isActive() const;
  void reset();
  void applyConstraints(Search* search, const Board& board, const BoardHistory& hist, Player pla);
  void advance(const Board& board, const BoardHistory& hist, Player pla, Loc playedLoc);
  void fillAvoidVectors(const Board& board, std::vector<int>& bVec, std::vector<int>& wVec);
 
 private:
  std::string dir;
  std::string bookJs;
  int bSizeX;
  int bSizeY;
  bool active;
  std::string currentFile;
  HtmlBookNode currentNode;
  double minRelWinPct;
  int currentSym;

  bool loadBookSizes();
  bool loadNode(const std::string& filePath);
  static bool parseNode(const std::string& content, HtmlBookNode& node);
  int composeSym(int sym1, int sym2) const;
  int getSymPos(int pos) const;
  int getInvSymPos(int pos) const;
};
#endif
