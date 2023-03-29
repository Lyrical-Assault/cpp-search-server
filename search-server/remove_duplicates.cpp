#include "remove_duplicates.h"

using namespace std::string_literals;

void RemoveDuplicates(SearchServer& search_server) {
    std::map<std::set<std::string>, int> unique_documents;
    std::vector<int> duplicates;
    for (int doc_id : search_server) {
        const auto& doc_words = search_server.GetWordFrequencies(doc_id);
        std::set<std::string> unique_words;
        for (const auto& [word, _] : doc_words) {
                unique_words.insert(word);
        }
        if (!unique_documents.count(unique_words)) {
            unique_documents[unique_words] = doc_id;
        } else {
            int existing_id = unique_documents[unique_words];
            if (doc_id > existing_id) {
                duplicates.push_back(doc_id);
                std::cout << "Found duplicate document id "s << doc_id << std::endl;
            } else {
                duplicates.push_back(existing_id);
                std::cout << "Found duplicate document id "s << existing_id << std::endl;
                unique_documents[unique_words] = doc_id;
            }
        }
    }
    for (int doc_id : duplicates) {
        search_server.RemoveDocument(doc_id);
    }
}
