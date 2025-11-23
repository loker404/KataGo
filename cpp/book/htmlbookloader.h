#ifndef BOOK_HTMLBOOKLOADER_H_
#define BOOK_HTMLBOOKLOADER_H_

#include "../core/global.h"
#include "book.h"
#include "../core/config_parser.h"
#include "../external/nlohmann_json/json.hpp"

class HtmlBookLoader {
public:
  // Load book from HTML directory
  static Book* loadFromHtmlDir(
    const std::string& dirName,
    const std::string& rulesLabel,
    const std::string& rulesLink,
    int bookVersion,
    const Board& initialBoard,
    Rules initialRules,
    Player initialPla,
    int repBound,
    BookParams params,
    double htmlMinVisits = 1.0
  );
  
  // Helper function to parse HTML file content
  static nlohmann::json parseHtmlFile(const std::string& filePath);
};

#endif // BOOK_HTMLBOOKLOADER_H_