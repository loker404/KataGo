#include "../program/htmlbook.h"

HtmlBookNavigator::HtmlBookNavigator(const std::string& d, double m)
  :dir(d),bookJs(),bSizeX(0),bSizeY(0),active(false),currentFile(),minRelWinPct(m),currentSym(0)
{
  loadBookSizes();
  reset();
}

bool HtmlBookNavigator::isActive() const { return active; }

void HtmlBookNavigator::reset() {
  if(bSizeX <= 0 || bSizeY <= 0) { active = false; return; }
  std::string root = dir + "/root/root.html";
  active = loadNode(root);
  currentSym = 0;
}

bool HtmlBookNavigator::loadBookSizes() {
  std::string jsPath = dir + "/book.js";
  try {
    bookJs = FileUtils::readFile(jsPath);
  } catch(...) { return false; }
  size_t ix = bookJs.find("const bSizeX");
  size_t iy = bookJs.find("const bSizeY");
  if(ix == std::string::npos || iy == std::string::npos) return false;
  size_t ixEq = bookJs.find('=', ix);
  size_t iyEq = bookJs.find('=', iy);
  if(ixEq == std::string::npos || iyEq == std::string::npos) return false;
  size_t ixEnd = bookJs.find(';', ixEq);
  size_t iyEnd = bookJs.find(';', iyEq);
  if(ixEnd == std::string::npos || iyEnd == std::string::npos) return false;
  bSizeX = Global::stringToInt(Global::trim(bookJs.substr(ixEq+1, ixEnd - (ixEq+1))));
  bSizeY = Global::stringToInt(Global::trim(bookJs.substr(iyEq+1, iyEnd - (iyEq+1))));
  return bSizeX > 0 && bSizeY > 0;
}

bool HtmlBookNavigator::loadNode(const std::string& filePath) {
  std::string content;
  try {
    content = FileUtils::readFile(filePath);
  } catch(...) { active = false; return false; }
  HtmlBookNode node;
  if(!parseNode(content, node)) { active = false; return false; }
  currentFile = filePath;
  currentNode = node;
  return active = true;
}

void HtmlBookNavigator::applyConstraints(Search* search, const Board& board, const BoardHistory& hist, Player pla) {
  (void)hist;
  (void)pla;
  if(!active) return;
  if(board.x_size != bSizeX || board.y_size != bSizeY) { active = false; return; }
  std::vector<int> bVec; std::vector<int> wVec;
  fillAvoidVectors(board,bVec,wVec);
  search->setAvoidMoveUntilByLoc(bVec, wVec);
}

void HtmlBookNavigator::advance(const Board& board, const BoardHistory& hist, Player pla, Loc playedLoc) {
  (void)hist;
  (void)pla;
  if(!active) return;
  if(board.x_size != bSizeX || board.y_size != bSizeY) { active = false; return; }
  if(playedLoc == Board::PASS_LOC) { active = false; return; }
  int x = Location::getX(playedLoc, board.x_size);
  int y = Location::getY(playedLoc, board.x_size);
  int posBoard = y * bSizeX + x;
  int posCanon = getInvSymPos(posBoard);
  auto it = currentNode.links.find(posCanon);
  if(it == currentNode.links.end()) { active = false; return; }
  std::string nextRel = it->second;
  auto itsPrev = currentNode.linkSyms.find(posCanon);
  int nextSym = currentSym;
  if(itsPrev != currentNode.linkSyms.end())
    nextSym = composeSym(itsPrev->second, currentSym);
  std::string nextPath = dir + "/" + nextRel;
  if(!loadNode(nextPath)) { active = false; return; }
  currentSym = nextSym;
}

bool HtmlBookNavigator::parseNode(const std::string& content, HtmlBookNode& node) const {
  size_t np = content.find("const nextPla");
  if(np != std::string::npos) {
    size_t eq = content.find('=', np);
    size_t end = content.find(';', eq);
    node.nextPla = Global::stringToInt(Global::trim(content.substr(eq+1, end - (eq+1))));
  }
  size_t linksStart = content.find("const links = {");
  if(linksStart == std::string::npos) return false;
  size_t linksEnd = content.find("};", linksStart);
  if(linksEnd == std::string::npos) return false;
  std::string linksBody = content.substr(linksStart, linksEnd - linksStart);
  size_t pos = linksBody.find(':');
  while(pos != std::string::npos) {
    // parse key
    // find start of number backwards
    size_t keyStart = linksBody.rfind('\n', pos);
    keyStart = keyStart == std::string::npos ? 0 : keyStart;
    std::string left = Global::trim(linksBody.substr(keyStart, pos - keyStart));
    // number may be like " 10"
    int key = -1;
    size_t numEnd = left.find_last_of("0123456789");
    if(numEnd != std::string::npos) {
      size_t numStart = numEnd;
      while(numStart > 0 && (left[numStart-1] >= '0' && left[numStart-1] <= '9')) numStart--;
      try { key = Global::stringToInt(left.substr(numStart, numEnd - numStart + 1)); } catch(...) {}
    }
    // parse value path
    size_t quote1 = linksBody.find('\'', pos);
    if(quote1 == std::string::npos) break;
    size_t quote2 = linksBody.find('\'', quote1+1);
    if(quote2 == std::string::npos) break;
    std::string rel = linksBody.substr(quote1+1, quote2 - (quote1+1));
    if(key >= 0) node.links[key] = rel;
    pos = linksBody.find(':', quote2);
  }
  size_t symsStart = content.find("const linkSyms = {");
  if(symsStart != std::string::npos) {
    size_t symsEnd = content.find("};", symsStart);
    if(symsEnd != std::string::npos) {
      std::string symsBody = content.substr(symsStart, symsEnd - symsStart);
      size_t p2 = symsBody.find(':');
      while(p2 != std::string::npos) {
        size_t keyStart = symsBody.rfind('\n', p2);
        keyStart = keyStart == std::string::npos ? 0 : keyStart;
        std::string left = Global::trim(symsBody.substr(keyStart, p2 - keyStart));
        int key = -1;
        size_t numEnd = left.find_last_of("0123456789");
        if(numEnd != std::string::npos) {
          size_t numStart = numEnd;
          while(numStart > 0 && (left[numStart-1] >= '0' && left[numStart-1] <= '9')) numStart--;
          try { key = Global::stringToInt(left.substr(numStart, numEnd - numStart + 1)); } catch(...) {}
        }
        size_t valStart = symsBody.find_first_of("0123456789", p2+1);
        if(valStart == std::string::npos) break;
        size_t valEnd = symsBody.find_first_not_of("0123456789", valStart);
        int val = Global::stringToInt(symsBody.substr(valStart, (valEnd==std::string::npos?symsBody.size():valEnd) - valStart));
        if(key >= 0) node.linkSyms[key] = val;
        p2 = symsBody.find(':', valEnd);
      }
    }
  }
  // detect pass and parse winpct by position
  size_t movesStart = content.find("const moves = [");
  if(movesStart != std::string::npos) {
    size_t movesEnd = content.find("];", movesStart);
    if(movesEnd != std::string::npos) {
      std::string movesBody = content.substr(movesStart, movesEnd - movesStart);
      if(movesBody.find("'pass'") != std::string::npos) node.hasPass = true;
      size_t objStart = movesBody.find('{');
      while(objStart != std::string::npos) {
        size_t objEnd = movesBody.find('}', objStart+1);
        if(objEnd == std::string::npos) break;
        std::string obj = movesBody.substr(objStart, objEnd - objStart + 1);
        size_t wlPos = obj.find("\"wl\"");
        double wlVal = 0.0;
        bool hasWl = false;
        if(wlPos != std::string::npos) {
          size_t colon = obj.find(':', wlPos);
          if(colon != std::string::npos) {
            size_t comma = obj.find(',', colon+1);
            std::string num = Global::trim(obj.substr(colon+1, (comma==std::string::npos?obj.size():comma) - (colon+1)));
            try { wlVal = Global::stringToDouble(num); hasWl = true; } catch(...) {}
          }
        }
        size_t xyPos = obj.find("\"xy\"");
        if(xyPos != std::string::npos && hasWl) {
          size_t lb = obj.find('[', xyPos);
          size_t rb = obj.find(']', lb+1);
          if(lb != std::string::npos && rb != std::string::npos) {
            std::string arr = obj.substr(lb+1, rb - (lb+1));
            size_t p = arr.find('[');
            while(p != std::string::npos) {
              size_t q = arr.find(']', p+1);
              if(q == std::string::npos) break;
              std::string pair = arr.substr(p+1, q - (p+1));
              std::vector<std::string> nums = Global::split(Global::trim(pair), ',');
              if(nums.size() >= 2) {
                int x = Global::stringToInt(Global::trim(nums[0]));
                int y = Global::stringToInt(Global::trim(nums[1]));
                int posIdx = y * bSizeX + x;
                double winPct = 0.0;
                if(node.nextPla == 1) winPct = 0.5 * (1.0 - wlVal);
                else winPct = 0.5 * (1.0 + wlVal);
                node.winpctByPos[posIdx] = winPct * 100.0;
              }
              p = arr.find('[', q+1);
            }
          }
        }
        if(hasWl && obj.find("\"move\"") != std::string::npos && obj.find("\"pass\"") != std::string::npos) {
          double winPct = 0.0;
          if(node.nextPla == 1) winPct = 0.5 * (1.0 - wlVal);
          else winPct = 0.5 * (1.0 + wlVal);
          node.passWinpct = winPct * 100.0;
        }
        objStart = movesBody.find('{', objEnd+1);
      }
    }
  }
  node.bSizeX = bSizeX; node.bSizeY = bSizeY;
  return node.links.size() > 0;
}

void HtmlBookNavigator::fillAvoidVectors(const Board& board, std::vector<int>& bVec, std::vector<int>& wVec) {
  bVec.assign(Board::MAX_ARR_SIZE,3);
  wVec.assign(Board::MAX_ARR_SIZE,3);
  if(!active) return;
  if(board.x_size != bSizeX || board.y_size != bSizeY) return;
  double best = -1e100;
  for(const auto& kv : currentNode.links) {
    int pos = kv.first;
    auto it = currentNode.winpctByPos.find(pos);
    if(it != currentNode.winpctByPos.end()) best = std::max(best, it->second);
  }
  for(const auto& kv : currentNode.links) {
    int pos = kv.first;
    double wp = -1e100;
    auto it = currentNode.winpctByPos.find(pos);
    if(it != currentNode.winpctByPos.end()) wp = it->second;
    bool allow = true;
    if(best > -1e50 && wp > -1e50)
      allow = (wp + 1e-9 >= best - minRelWinPct);
    int symPos = getSymPos(pos);
    int x = symPos % bSizeX;
    int y = symPos / bSizeX;
    Loc loc = Location::getLoc(x,y,board.x_size);
    if(allow) { bVec[loc] = 0; wVec[loc] = 0; }
  }
  if(currentNode.hasPass) {
    bool allowPass = true;
    if(currentNode.passWinpct > -1e50 && best > -1e50)
      allowPass = (currentNode.passWinpct + 1e-9 >= best - minRelWinPct);
    if(allowPass) {
      bVec[Board::PASS_LOC] = 0;
      wVec[Board::PASS_LOC] = 0;
    }
  }
}

int HtmlBookNavigator::composeSym(int sym1, int sym2) const {
  if(sym1 & 0x4)
    sym2 = (sym2 & 0x4) | ((sym2 & 0x2) >> 1) | ((sym2 & 0x1) << 1);
  return sym1 ^ sym2;
}

int HtmlBookNavigator::getSymPos(int pos) const {
  int y = pos / bSizeX;
  int x = pos % bSizeX;
  int sym = currentSym;
  if(sym & 1)
    y = bSizeY-1-y;
  if(sym & 2)
    x = bSizeX-1-x;
  if((sym & 4) && bSizeX == bSizeY) {
    int tmp = x; x = y; y = tmp;
  }
  return x + y*bSizeX;
}

int HtmlBookNavigator::getInvSymPos(int pos) const {
  int y = pos / bSizeX;
  int x = pos % bSizeX;
  int sym = currentSym;
  if((sym & 4) && bSizeX == bSizeY) {
    int tmp = x; x = y; y = tmp;
  }
  if(sym & 1)
    y = bSizeY-1-y;
  if(sym & 2)
    x = bSizeX-1-x;
  return x + y*bSizeX;
}
