#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> docs(queries.size());
    std::transform(std::execution::par,
                   queries.begin(), queries.end(),
                   docs.begin(),
                   [&search_server](const std::string& query) {
                       return search_server.FindTopDocuments(query);
                   });
    return docs;
}

std::vector<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    std::vector<Document> docs;
    for(const auto& vec_docs : ProcessQueries(search_server, queries)) {
        docs.insert(docs.end(), vec_docs.begin(), vec_docs.end());
    }
    return docs;
}