#pragma once

#include <vector>
#include "document.h"
#include <execution>
#include "search_server.h"
#include <algorithm>

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);

std::vector<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);
