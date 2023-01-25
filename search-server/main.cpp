#include <iostream>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define RUN_TEST(func)  RunTestImpl(func, #func)

template <typename Func>
void RunTestImpl(Func func, const string& str_func){
    func();
    cerr << str_func << " OK"s << endl;
}

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / static_cast<double>(words.size());
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus document_status) const {
        return FindTopDocuments(raw_query,[document_status](int document_id, DocumentStatus status, int rating) { return status == document_status;});
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1, "Only one document was added!"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Wrong document id!"s);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "There should be no documents on query which contains only stop-word!"s);
    }
}
//Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDoc(){
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "black dog"s, DocumentStatus::ACTUAL, {1,2});
    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL_HINT(found_docs.size(), 1, "Wrong number of documents!"s);
}
//Поддержка стоп-слов. Стоп-слова исключаются из текста документов.
void TestStopWords(){
    SearchServer server;
    server.SetStopWords("in the a"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "a black dog"s, DocumentStatus::ACTUAL, {1, 2});
    ASSERT_HINT(server.FindTopDocuments("in a"s).empty(), "There are stop-words in the document!"s);
}
//Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
void TestMinusWords(){
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "dog in the city"s, DocumentStatus::ACTUAL, {1, 2});
    const auto found_docs = server.FindTopDocuments("cat and -dog in the city"s);
    ASSERT_EQUAL_HINT(found_docs.size(), 1, "Where are documents that contains minus-words!"s);
    const auto found_docs_2 = server.FindTopDocuments("cat and dog in the city"s);
    ASSERT_EQUAL_HINT(found_docs_2.size(), 2, "Where are documents that contains minus-words!"s);
}
//Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatching(){
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto [words, status] = server.MatchDocument("-cat city"s, 1);
    ASSERT_EQUAL_HINT(words.size(), 0, "Document contains minus-word!"s);
    server.AddDocument(2, "black dog in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto [words_2, status_2] = server.MatchDocument("dog city"s, 2);
    ASSERT_EQUAL(words_2.size(), 2);
}
//Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSorting(){
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "dog in the city"s, DocumentStatus::ACTUAL, {1, 2});
    const auto found_docs = server.FindTopDocuments("city"s);
    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];
    ASSERT_HINT(doc0.relevance >= doc1.relevance, "Results are sorted incorrectly!"s);
}
//Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestComputeRating(){
    const vector<int> ratings = {1, 2, 3};
    const int res_rating = accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size());
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("cat"s);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL_HINT(doc0.rating, res_rating, "Rating is compute incorrectly!"s);
}
//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestFiltering(){
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "black dog and white cat"s, DocumentStatus::BANNED, {3, 4});
    const auto found_docs = server.FindTopDocuments("cat "s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_HINT(!found_docs.empty(), "Results of filtering are incorrect!"s);
    const auto found_docs_2 = server.FindTopDocuments("cat "s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
    ASSERT_HINT(!found_docs_2.empty(), "Results of filtering are incorrect!"s);
}
//Поиск документов, имеющих заданный статус.
void TestStatus(){
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "black dog and white cat"s, DocumentStatus::BANNED, {3, 4});
    const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL_HINT(found_docs.size(), 1, "\"Wrong number of documents with given status!"s);
    const auto found_docs_2 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
    ASSERT_EQUAL_HINT(found_docs_2.size(), 1, "Wrong number of documents with given status!"s);
    const auto found_docs_3 = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL_HINT(found_docs_3.size(), 0, "There are no documents with this status!"s);

}
//Корректное вычисление релевантности найденных документов.
void TestComputeRelevance(){
    const double res_relevance = 0.173287;
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "white cat and black dog"s, DocumentStatus::BANNED, {3, 4});
    const auto found_docs = server.FindTopDocuments("city"s);
    const Document& doc0 = found_docs[0];
    ASSERT_HINT(abs(doc0.relevance - res_relevance) < 1e-6, "Relevance is compute incorrectly!"s);
}
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDoc);
    RUN_TEST(TestStopWords);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatching);
    RUN_TEST(TestSorting);
    RUN_TEST(TestComputeRating);
    RUN_TEST(TestFiltering);
    RUN_TEST(TestStatus);
    RUN_TEST(TestComputeRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
}
