#include "BookLoaderHtml.h"
#include "../core/Hash.h"
#include "../core/Logging.h"
#include "../core/Random.h"
#include "../core/Time.h"
#include "../game/Board.h"
#include "../game/BoardHistory.h"
#include "../game/GameState.h"
#include "../parsing/Json.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>

using namespace std;

namespace Book {

// Helper function to extract moves from HTML content
vector<Loc> parseHtmlMoves(const string& htmlContent, const string& boardXSize, const string& boardYSize) {
    vector<Loc> moves;
    
    // Extract move data from the JavaScript moves array
    size_t movesStart = htmlContent.find("var moves = [");
    if (movesStart == string::npos) {
        return moves;
    }
    
    size_t movesEnd = htmlContent.find("];", movesStart);
    if (movesEnd == string::npos) {
        return moves;
    }
    
    movesEnd += 2; // include the ]; 
    string movesSection = htmlContent.substr(movesStart, movesEnd - movesStart);
    
    // Find all move entries in the format ["A1", "B2", ...]
    regex moveRegex(R"(\[\"([A-Z][0-9]+)\"\s*,\s*\"([A-Z][0-9]+)\"\s*\])");
    smatch match;
    string::const_iterator searchStart(movesSection.cbegin());
    
    while (regex_search(searchStart, movesSection.cend(), match, moveRegex)) {
        string blackMove = match[1].str();
        string whiteMove = match[2].str();
        
        // Convert coordinates to Loc
        Loc blackLoc = Board::locFromSGFXY(
            blackMove[0] - 'A', 
            stoi(blackMove.substr(1)) - 1,
            stoi(boardXSize), 
            stoi(boardYSize)
        );
        
        Loc whiteLoc = Board::locFromSGFXY(
            whiteMove[0] - 'A', 
            stoi(whiteMove.substr(1)) - 1,
            stoi(boardXSize), 
            stoi(boardYSize)
        );
        
        moves.push_back(blackLoc);
        moves.push_back(whiteLoc);
        
        searchStart = match.suffix().first;
    }
    
    return moves;
}

// Helper function to read file content
string readFileContent(const string& filepath) {
    ifstream file(filepath);
    if (!file.is_open()) {
        return "";
    }
    
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

vector<MoveState> loadFromHtmlDir(const string& bookDirPath, int maxDepth, Logger& logger) {
    vector<MoveState> bookMoves;
    logger.write("Loading opening book from HTML directory: " + bookDirPath);
    
    // Check if the directory exists
    if (!filesystem::exists(bookDirPath) || !filesystem::is_directory(bookDirPath)) {
        logger.write("ERROR: HTML book directory does not exist or is not a directory: " + bookDirPath);
        return bookMoves;
    }
    
    string boardXSize = "19"; // Default value
    string boardYSize = "19"; // Default value
    
    // Look for root.html or index.html in the directory
    vector<string> possibleRootFiles = {"root.html", "index.html", "Root.html", "Index.html"};
    string rootFilePath = "";
    
    for (const string& filename : possibleRootFiles) {
        string fullPath = bookDirPath + "/" + filename;
        if (filesystem::exists(fullPath)) {
            rootFilePath = fullPath;
            break;
        }
    }
    
    if (rootFilePath.empty()) {
        logger.write("ERROR: No root HTML file found in directory: " + bookDirPath);
        return bookMoves;
    }
    
    // Read the root HTML file to get board size and other configuration
    string rootContent = readFileContent(rootFilePath);
    if (rootContent.empty()) {
        logger.write("ERROR: Could not read root HTML file: " + rootFilePath);
        return bookMoves;
    }
    
    // Extract board size from HTML content
    regex xSizeRegex(R"(var boardXSize = (\d+);)");
    regex ySizeRegex(R"(var boardYSize = (\d+);)");
    smatch match;
    
    if (regex_search(rootContent, match, xSizeRegex)) {
        boardXSize = match[1].str();
    }
    
    if (regex_search(rootContent, match, ySizeRegex)) {
        boardYSize = match[1].str();
    }
    
    logger.write("Detected board size: " + boardXSize + "x" + boardYSize);
    
    // Parse moves from the root file
    vector<Loc> rootMoves = parseHtmlMoves(rootContent, boardXSize, boardYSize);
    
    // Create initial game state
    Board::PositionSampleBufPos sampleBufPos;
    Board::PositionSampleBufNeg sampleBufNeg;
    Board::PositionSampleBufMaybeWin sampleBufMaybeWin;
    
    int boardX = stoi(boardXSize);
    int boardY = stoi(boardYSize);
    
    GameState initialState(boardX, boardY, sampleBufPos, sampleBufNeg, sampleBufMaybeWin);
    BoardHistory initialHist(initialState);
    
    // Add moves from root file
    GameState currentState = initialState;
    BoardHistory currentHist = initialHist;
    
    for (size_t i = 0; i < rootMoves.size() && i < static_cast<size_t>(maxDepth); i++) {
        Loc moveLoc = rootMoves[i];
        Player movePlayer = (i % 2 == 0) ? C_BLACK : C_WHITE;
        
        if (currentState.addMoveLoc(moveLoc, movePlayer, currentHist, true, true, false)) {
            // Calculate position hash after the move
            uint64_t posHash = Hash::hashPosition(currentState);
            
            // Create MoveState entry
            MoveState moveState;
            moveState.posHash = posHash;
            moveState.loc = moveLoc;
            moveState.prob = 1.0f;  // Default probability for HTML moves
            
            bookMoves.push_back(moveState);
            
            logger.write("Added move: " + Board::locToSGF(currentState, moveLoc) + 
                        " for player " + (movePlayer == C_BLACK ? "B" : "W"));
        } else {
            logger.write("Failed to add move: " + Board::locToSGF(currentState, moveLoc));
            break;
        }
    }
    
    // Recursively process subdirectories
    for (const auto& entry : filesystem::recursive_directory_iterator(bookDirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".html") {
            string htmlFilePath = entry.path().string();
            if (htmlFilePath != rootFilePath) {  // Skip the root file as we already processed it
                string htmlContent = readFileContent(htmlFilePath);
                vector<Loc> moves = parseHtmlMoves(htmlContent, boardXSize, boardYSize);
                
                // Process moves from this HTML file
                GameState fileState = initialState;
                BoardHistory fileHist = initialHist;
                
                for (size_t i = 0; i < moves.size() && i < static_cast<size_t>(maxDepth); i++) {
                    Loc moveLoc = moves[i];
                    Player movePlayer = (i % 2 == 0) ? C_BLACK : C_WHITE;
                    
                    if (fileState.addMoveLoc(moveLoc, movePlayer, fileHist, true, true, false)) {
                        uint64_t posHash = Hash::hashPosition(fileState);
                        
                        MoveState moveState;
                        moveState.posHash = posHash;
                        moveState.loc = moveLoc;
                        moveState.prob = 1.0f;  // Default probability
                        
                        bookMoves.push_back(moveState);
                        
                        logger.write("Added move from " + entry.path().filename().string() + ": " + 
                                    Board::locToSGF(fileState, moveLoc) + 
                                    " for player " + (movePlayer == C_BLACK ? "B" : "W"));
                    } else {
                        logger.write("Failed to add move from " + entry.path().filename().string() + ": " + 
                                    Board::locToSGF(fileState, moveLoc));
                        break;
                    }
                }
            }
        }
    }
    
    logger.write("Loaded " + to_string(bookMoves.size()) + " moves from HTML directory");
    return bookMoves;
}

}