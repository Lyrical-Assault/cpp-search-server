#include "search_server.h"

using namespace std::string_literals;

SearchServer::SearchServer(const std::string& stop_words_text)
        : SearchServer(std::string_view(stop_words_text)) {}

SearchServer::SearchServer(std::string_view stop_words_text)
        : SearchServer(
        SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    words_.emplace_back(document);
    const auto words = SplitIntoWordsNoStop(words_.back());
    const double inv_word_count = 1.0 / words.size();
    for (std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freq_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy& policy, std::string_view raw_query, int document_id) const {
    const auto status = documents_.at(document_id).status;
    const auto query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;
    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {matched_words, status};
        }
    }
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy& policy, std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query, false);
    std::vector<std::string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    if(!std::any_of(policy, query.minus_words.begin(),query.minus_words.end(),
                    [this, document_id](auto word) {
                        return word_to_document_freqs_.at(word).count(document_id);}))
    {
        auto matched_words_end = std::copy_if(policy,
                                              query.plus_words.begin(), query.plus_words.end(),
                                              matched_words.begin(),
                                              [this, document_id](auto word){
                                                  return word_to_document_freqs_.at(word).count(document_id);});
        std::sort(matched_words.begin(), matched_words_end);
        matched_words.erase(std::unique(matched_words.begin(),
                                        matched_words_end), matched_words.end());
    }
    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + std::string(text) + " is invalid"s);
    }
    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, const bool is_seq) const {
    Query result;
    for (std::string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if(is_seq){
        std::sort(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(std::unique(result.plus_words.begin(), result.plus_words.end()), result.plus_words.end());
        std::sort(result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(std::unique(result.minus_words.begin(), result.minus_words.end()), result.minus_words.end());
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

typename std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

typename std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if(lower_bound(document_ids_.begin(), document_ids_.end(), document_id) != document_ids_.end()) {
        return word_freq_.at(document_id);
    } else {
        static const std::map<std::string_view, double> empty_map;
        return empty_map;
    }
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy& policy, int document_id) {
    for(auto [key, value] : word_freq_.at(document_id)) {
        word_to_document_freqs_.at(key).erase(document_id);
    }
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    word_freq_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& policy, int document_id) {
    auto& word_freq = word_freq_.at(document_id);
    std::vector<std::string_view> document_words(word_freq.size());
    std::transform(policy,
                   word_freq.begin(), word_freq.end(),
                   document_words.begin(),
                   [](const auto& word){
                       return word.first;});
    std::for_each(policy,
                  document_words.begin(), document_words.end(),
                  [document_id, this](std::string_view word){
                      word_to_document_freqs_.at(word).erase(document_id);});
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    word_freq_.erase(document_id);
}