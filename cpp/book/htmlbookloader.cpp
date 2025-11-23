#include "htmlbookloader.h"
#include "../core/fileutils.h"
#include "../core/makedir.h"
#include "../game/boardhistory.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <set>

using nlohmann::json;

// Helper function to extract JavaScript variables from HTML
static std::string extractJsVariable(const std::string& content, const std::string& varName) {
  std::regex pattern(varName + "\\s*=\\s*([^;]+);");
  std::smatch match;
  if (std::regex_search(content, match, pattern)) {
    std::string result = match[1].str();
    // Remove leading/trailing whitespace
    result.erase(0, result.find_first_not_of(" \t\n\r"));
    result.erase(result.find_last_not_of(" \t\n\r") + 1);
    return result;
  }
  return "";
}

// Helper function to extract JavaScript object content
static std::string extractJsObject(const std::string& content, const std::string& varName) {
  std::regex pattern(varName + "\\s*=\\s*(\\{[^}]*\\}|\\[[^\\]]*\\]|\"[^\"]*\"|[^,;]+)[,;]");
  std::smatch match;
  if (std::regex_search(content, match, pattern)) {
    std::string result = match[1].str();
    // Remove leading/trailing whitespace
    result.erase(0, result.find_first_not_of(" \t\n\r"));
    result.erase(result.find_last_not_of(" \t\n\r") + 1);
    return result;
  }
  return "";
}

nlohmann::json HtmlBookLoader::parseHtmlFile(const std::string& filePath) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    throw StringError("Could not open HTML file: " + filePath);
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();
  file.close();
  
  // Extract JavaScript variables from the HTML file
  nlohmann::json result = nlohmann::json::object();
  
  // Extract data variables from the HTML file
  std::string nextPlaStr = extractJsVariable(content, "nextPla");
  if (!nextPlaStr.empty()) {
    try {
      result["nextPla"] = std::stoi(nextPlaStr);
    } catch (...) {
      // Handle error if needed
    }
  }
  
  std::string pLink = extractJsVariable(content, "pLink");
  if (!pLink.empty()) {
    result["pLink"] = pLink;
  }
  
  std::string pSymStr = extractJsVariable(content, "pSym");
  if (!pSymStr.empty()) {
    try {
      result["pSym"] = std::stoi(pSymStr);
    } catch (...) {
      // Handle error if needed
    }
  }
  
  std::string boardStr = extractJsVariable(content, "board");
  if (!boardStr.empty()) {
    // Parse the board array
    try {
      // Replace single quotes with double quotes for JSON compatibility
      std::string boardJson = boardStr;
      std::replace(boardJson.begin(), boardJson.end(), '\'', '"');
      result["board"] = json::parse(boardJson);
    } catch (...) {
      // Handle error if needed
    }
  }
  
  std::string linksStr = extractJsVariable(content, "links");
  if (!linksStr.empty()) {
    try {
      // Replace single quotes with double quotes for JSON compatibility
      std::string linksJson = linksStr;
      std::replace(linksJson.begin(), linksJson.end(), '\'', '"');
      result["links"] = json::parse(linksJson);
    } catch (...) {
      // Handle error if needed
    }
  }
  
  std::string linkSymsStr = extractJsVariable(content, "linkSyms");
  if (!linkSymsStr.empty()) {
    try {
      // Replace single quotes with double quotes for JSON compatibility
      std::string linkSymsJson = linkSymsStr;
      std::replace(linkSymsJson.begin(), linkSymsJson.end(), '\'', '"');
      result["linkSyms"] = json::parse(linkSymsJson);
    } catch (...) {
      // Handle error if needed
    }
  }
  
  std::string movesStr = extractJsVariable(content, "moves");
  if (!movesStr.empty()) {
    try {
      // Replace single quotes with double quotes for JSON compatibility
      std::string movesJson = movesStr;
      std::replace(movesJson.begin(), movesJson.end(), '\'', '"');
      result["moves"] = json::parse(movesJson);
    } catch (...) {
      // Handle error if needed
    }
  }
  
  return result;
}

// Helper function to recursively traverse HTML directory
static void traverseHtmlDirectory(
  const std::string& dirName,
  const std::string& currentPath,
  std::set<std::string>& processedFiles,
  std::function<void(const std::string&)> processFile
) {
  std::string fullPath = dirName + "/" + currentPath;
  
  // Check if this is a file
  if (fullPath.length() > 5 && fullPath.substr(fullPath.length() - 5) == ".html") {
    if (processedFiles.find(currentPath) == processedFiles.end()) {
      processedFiles.insert(currentPath);
      processFile(fullPath);
    }
    return;
  }
  
  // If it's a directory, process its contents
  for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
    std::string relativePath = entry.path().string().substr(dirName.length() + 1);
    
    if (entry.is_regular_file() && relativePath.length() > 5 && 
        relativePath.substr(relativePath.length() - 5) == ".html") {
      if (processedFiles.find(relativePath) == processedFiles.end()) {
        processedFiles.insert(relativePath);
        processFile(entry.path().string());
      }
    }
  }
}

Book* HtmlBookLoader::loadFromHtmlDir(
  const std::string& dirName,
  const std::string& rulesLabel,
  const std::string& rulesLink,
  int bookVersion,
  const Board& initialBoard,
  Rules initialRules,
  Player initialPla,
  int repBound,
  BookParams params,
  double htmlMinVisits
) {
  // Create a new book with the provided parameters
  Book* book = new Book(bookVersion, initialBoard, initialRules, initialPla, repBound, params);
  
  // Keep track of processed files to avoid duplicates
  std::set<std::string> processedFiles;
  
  // Process all HTML files in the directory
  for (const auto& entry : std::filesystem::recursive_directory_iterator(dirName)) {
    if (entry.is_regular_file() && 
        entry.path().extension() == ".html") {
      
      std::string filePath = entry.path().string();
      std::string relativePath = filePath.substr(dirName.length() + 1);
      
      if (processedFiles.find(relativePath) != processedFiles.end()) {
        continue; // Already processed
      }
      
      processedFiles.insert(relativePath);
      
      try {
        nlohmann::json fileData = parseHtmlFile(filePath);
        
        if (fileData.contains("board") && fileData.contains("moves")) {
          // Extract board data
          auto boardArray = fileData["board"];
          if (boardArray.is_array() && boardArray.size() == initialBoard.x_size * initialBoard.y_size) {
            // Create a board state from the array data
            Board board = initialBoard;
            for (int i = 0; i < boardArray.size(); i++) {
              int x = i % initialBoard.x_size;
              int y = i / initialBoard.x_size;
              Loc loc = Location::getLoc(x, y, initialBoard.x_size);
              Player color = (Player)boardArray[i].get<int>();
              board.colors[loc] = color;
            }
            
            // Get the player to play
            Player nextPla = P_BLACK;
            if (fileData.contains("nextPla")) {
              nextPla = (Player)fileData["nextPla"].get<int>();
            }
            
            // Calculate hash for this board state
            BoardHistory hist(board, nextPla, initialRules, 0);
            BookHash hash;
            int symmetryToAlign;
            std::vector<int> symmetries;
            BookHash::getHashAndSymmetry(hist, repBound, hash, symmetryToAlign, symmetries, bookVersion);
            
            // Check if node already exists
            BookNode* node = book->get(hash);
            if (node == nullptr) {
              node = new BookNode(hash, book, nextPla, symmetries);
              book->add(hash, node);
            }
            
            // Process moves data if available
            if (fileData.contains("moves") && fileData["moves"].is_array()) {
              auto movesArray = fileData["moves"];
              
              for (size_t i = 0; i < movesArray.size(); i++) {
                auto moveData = movesArray[i];
                
                if (moveData.is_object()) {
                  // Extract move information
                  std::string moveStr = "";
                  if (moveData.contains("move")) {
                    moveStr = moveData["move"].get<std::string>();
                  }
                  
                  // Handle different types of moves
                  Loc moveLoc = Board::NULL_LOC;
                  if (moveStr == "pass") {
                    moveLoc = Board::PASS_LOC;
                  } else if (moveStr == "other") {
                    continue; // Skip "other" moves which represent not expanding
                  } else if (moveData.contains("xy") && moveData["xy"].is_array()) {
                    // Get move from coordinates
                    auto xyArray = moveData["xy"];
                    if (xyArray.is_array() && xyArray.size() > 0) {
                      auto coords = xyArray[0];
                      if (coords.is_array() && coords.size() >= 2) {
                        int x = coords[0].get<int>();
                        int y = coords[1].get<int>();
                        moveLoc = Location::getLoc(x, y, initialBoard.x_size);
                      }
                    }
                  }
                  
                  if (moveLoc != Board::NULL_LOC && moveLoc != Board::PASS_LOC) {
                    // Verify that the move is legal on the current board
                    if (board.colors[moveLoc] == C_EMPTY) {
                      // Extract move values
                      double policy = 0.0;
                      double winLoss = 0.0;
                      double scoreMean = 0.0;
                      double visits = 0.0;
                      
                      if (moveData.contains("p")) {
                        policy = moveData["p"].get<double>();
                      }
                      if (moveData.contains("wl")) {
                        winLoss = moveData["wl"].get<double>();
                      }
                      if (moveData.contains("ssM")) {
                        scoreMean = moveData["ssM"].get<double>();
                      }
                      if (moveData.contains("v")) {
                        visits = moveData["v"].get<double>();
                      }
                      
                      // Create book move if it doesn't exist
                      if (node->moves.find(moveLoc) == node->moves.end()) {
                        // We need to create a hash for the child position
                        // For now, we'll use a placeholder - in a full implementation,
                        // we would need to calculate the actual hash of the resulting position
                        BookHash childHash; // This is a placeholder
                        BookMove bookMove(moveLoc, 0, childHash, policy);
                        node->moves[moveLoc] = bookMove;
                        
                        // Update the node's values based on the move data
                        if (node->thisValuesNotInBook.visits < visits) {
                          node->thisValuesNotInBook.visits = visits;
                        }
                        if (policy > node->thisValuesNotInBook.maxPolicy) {
                          node->thisValuesNotInBook.maxPolicy = policy;
                        }
                      }
                    }
                  } else if (moveLoc == Board::PASS_LOC) {
                    // Handle pass move
                    if (node->moves.find(Board::PASS_LOC) == node->moves.end()) {
                      BookHash childHash; // Placeholder
                      BookMove bookMove(Board::PASS_LOC, 0, childHash, 
                                      moveData.contains("p") ? moveData["p"].get<double>() : 0.0);
                      node->moves[Board::PASS_LOC] = bookMove;
                    }
                  }
                }
              }
            }
          }
        }
      } catch (const std::exception& e) {
        // Log error but continue processing other files
        // In a real implementation, we might want to log this error
      }
    }
  }
  
  return book;
}