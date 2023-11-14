#pragma once

#include "document.h"
#include "search_server.h"

#include <algorithm>
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::string request;
        bool isEmptyRequest;
    };

    std::deque<QueryResult> requests_;

    const static int min_in_day_ = 1440;

    int seconds_ = 0;

    const SearchServer& search_server_ ;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    const std::vector<Document> matched_documents = search_server_.FindTopDocuments(raw_query, document_predicate);
    requests_.push_front({raw_query, matched_documents.empty()});
    ++seconds_;
    if(seconds_ > min_in_day_) {
        requests_.pop_back();
    }
    return matched_documents;
}