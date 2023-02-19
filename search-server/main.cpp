#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cassert>
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

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
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
    Document() = default;

    Document(int id, double relevance, int rating)
            : id(id)
            , relevance(relevance)
            , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

   SearchServer() = default;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
            : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for(const auto& stop_word : stop_words){
            if(!IsValidWord(stop_word)){
                throw invalid_argument ("Stop word is not valid!"s);
            }
        }
    }

    explicit SearchServer(const string& stop_words_text): SearchServer(SplitIntoWords(stop_words_text))
    {
        if(!IsValidWord(stop_words_text)){
            throw invalid_argument ("Stop word is not valid!"s);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if(document_id < 0 || documents_.count(document_id) || !IsValidWord(document)){
            throw invalid_argument ("Error from AddDocument!"s);
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        docs_id_.push_back(document_id);
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);
        if(!query){
            throw invalid_argument ("Error from FindTopDocuments!"s);
        }
        auto matched_documents = FindAllDocuments(query.value(), document_predicate);
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query,
                                ([status](int document_id, DocumentStatus document_status, int rating) {
                                    return document_status == status;
                                }));
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    int GetDocumentId(int index) const {
        return docs_id_.at(index);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        vector<string> matched_words;
        const auto query = ParseQuery(raw_query);
        if(!query){
            throw invalid_argument ("Error from MatchDocument!"s);
        }
        for (const string& word : query.value().plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.value().minus_words) {
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
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> docs_id_;

    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

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

    optional<QueryWord> ParseQueryWord(string text) const {
        if (text.empty()) {
            return nullopt;
        }
        string word = text;
        bool is_minus = false;
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
            return nullopt;
        }
        return QueryWord{word, is_minus, IsStopWord(word)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    optional<Query> ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const auto query_word = ParseQueryWord(word);
            if(!query_word){
                return nullopt;
            }
            if (!query_word.value().is_stop) {
                if (query_word.value().is_minus) {
                    query.minus_words.insert(query_word.value().data);
                } else {
                    query.plus_words.insert(query_word.value().data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
                                      DocumentPredicate document_predicate) const {
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
        ASSERT_EQUAL_HINT(static_cast<int>(found_docs.size()), 1, "Only one document was added!"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Wrong document id!"s);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "There should be no documents on query which contains only stop-word!"s);
    }
}
//Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDoc(){
    SearchServer server("in the"s);
    const auto found_docs = server.FindTopDocuments("city"s);
    ASSERT_EQUAL_HINT(static_cast<int>(found_docs.size()), 0, "Wrong number of documents!"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1, "Wrong number of documents!"s);
    server.AddDocument(2, "black dog in the city"s, DocumentStatus::ACTUAL, {1, 2});
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 2, "Wrong number of documents!"s);
}
//Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
void TestMinusWords(){
    SearchServer server("in the"s);
    const int doc0_id = 1;
    const int doc1_id = 2;
    server.AddDocument(doc0_id, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(doc1_id, "dog in the city"s, DocumentStatus::ACTUAL, {1, 2});
    const auto found_docs = server.FindTopDocuments("cat and -dog"s);
    ASSERT_EQUAL_HINT(static_cast<int>(found_docs.size()), 1, "Where are documents that contains minus-words!"s);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL_HINT(doc0.id, doc0_id, "Wrong document id!"s);
    const auto found_docs_2 = server.FindTopDocuments("cat and dog"s);
    ASSERT_EQUAL_HINT(static_cast<int>(found_docs_2.size()), 2, "Where are documents that contains minus-words!"s);
    const Document& doc0_2 = found_docs_2[0];
    const Document& doc1_2 = found_docs_2[1];
    ASSERT_EQUAL_HINT(doc0_2.id, doc0_id, "Wrong document id!"s);
    ASSERT_EQUAL_HINT(doc1_2.id, doc1_id, "Wrong document id!"s);
}
//Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatching(){
    SearchServer server("in the"s);
    const vector<string> test_words = {"black"s, "dog"s};
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto [words, status] = server.MatchDocument("-cat in city"s, 1);
    ASSERT_EQUAL_HINT(static_cast<int>(words.size()), 0, "Document contains minus-word!"s);
    ASSERT(status == DocumentStatus::ACTUAL);
    server.AddDocument(2, "black dog in the city"s, DocumentStatus::BANNED, {1, 2, 3});
    const auto [words_2, status_2] = server.MatchDocument("the black dog"s, 2);
    ASSERT_EQUAL(static_cast<int>(words_2.size()), 2);
    ASSERT(words_2 == test_words);
    ASSERT(status_2 == DocumentStatus::BANNED);
}
//Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSorting(){
    SearchServer server("in the"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "dog in the city"s, DocumentStatus::ACTUAL, {1, 2});
    const auto found_docs = server.FindTopDocuments("city"s);
    ASSERT_EQUAL(static_cast<int>(found_docs.size()), 2);
    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];
    ASSERT_HINT(doc0.relevance >= doc1.relevance, "Results are sorted incorrectly!"s);
}
//Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestComputeRating(){
    const vector<int> ratings = {1, 2, 3};
    const int res_rating = accumulate(ratings.begin(), ratings.end(), 0)/static_cast<int>(ratings.size());
    SearchServer server("in the"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(static_cast<int>(found_docs.size()), 1);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL_HINT(doc0.rating, res_rating, "Rating is compute incorrectly!"s);
}
//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestFiltering(){
    const int doc0_id = 1;
    const int doc1_id = 2;
    SearchServer server("in the"s);
    server.AddDocument(doc0_id, "cat in the city"s, DocumentStatus::BANNED, {1, 2, 3});
    server.AddDocument(doc1_id, "black dog and white cat"s, DocumentStatus::ACTUAL, {3, 4});
    const auto found_docs = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL(static_cast<int>(found_docs.size()), 1);
    const Document& doc1 = found_docs[0];
    ASSERT_EQUAL_HINT(doc1.id, doc1_id, "Wrong document id!"s);
    const auto found_docs_2 = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
    ASSERT_EQUAL(static_cast<int>(found_docs_2.size()), 1);
    const Document& doc0 = found_docs_2[0];
    ASSERT_EQUAL_HINT(doc0.id, doc0_id, "Wrong document id!"s);
}
//Поиск документов, имеющих заданный статус.
void TestStatus(){
    const int doc0_id = 1;
    const int doc1_id = 2;
    SearchServer server("in the"s);
    server.AddDocument(doc0_id, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(doc1_id, "black dog and white cat"s, DocumentStatus::BANNED, {3, 4});
    const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL_HINT(static_cast<int>(found_docs.size()), 1, "Wrong number of documents with given status!"s);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL_HINT(doc0.id, doc0_id, "Wrong document id!"s);
    const auto found_docs_2 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
    ASSERT_EQUAL_HINT(static_cast<int>(found_docs_2.size()), 1, "Wrong number of documents with given status!"s);
    const Document& doc1 = found_docs_2[0];
    ASSERT_EQUAL_HINT(doc1.id, doc1_id, "Wrong document id!"s);
    const auto found_docs_3 = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL_HINT(static_cast<int>(found_docs_3.size()), 0, "There are no documents with this status!"s);
}
//Корректное вычисление релевантности найденных документов.
void TestComputeRelevance(){
    SearchServer server("in the"s);
    const int doc0_id = 1;
    const int doc1_id = 2;
    const string raw_query = "fluffy cat"s;
    const string content1 = "white cat fluffy tail"s;
    const string content2 = "black dog beautiful eyes"s;
    server.AddDocument(doc0_id, content1, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(doc1_id, content2, DocumentStatus::ACTUAL, {3, 4});
    const auto found_docs = server.FindTopDocuments(raw_query);
    ASSERT_EQUAL(static_cast<int>(found_docs.size()), 1);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL_HINT(doc0.id, doc0_id, "Wrong document id!"s);
    ASSERT_HINT(abs(doc0.relevance - (log(server.GetDocumentCount() * 1.0 / 1) * (2.0 / 4))) < EPSILON, "Relevance is compute incorrectly!"s);
}
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDoc);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatching);
    RUN_TEST(TestSorting);
    RUN_TEST(TestComputeRating);
    RUN_TEST(TestFiltering);
    RUN_TEST(TestStatus);
    RUN_TEST(TestComputeRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}
int main() {
    try{
        SearchServer search_server("и в на"s);
        SearchServer invalid_stop_words("и в н\x12а"s);
        search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
        search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, {1, 3, 2});
        search_server.FindTopDocuments("пу\x12шистый"s);
        search_server.FindTopDocuments("пушистый --кот"s);
        search_server.FindTopDocuments("иван-чай"s);
        search_server.FindTopDocuments("пушистый -"s);
        search_server.MatchDocument("пу\x12шистый"s, 1);
        search_server.MatchDocument("пушистый --кот"s, 1);
        search_server.MatchDocument("иван-чай"s, 1);
        search_server.MatchDocument("пушистый -"s, 1);
        search_server.GetDocumentId(-1);
    }
    catch(const invalid_argument& error){
        cout << "Invalid_argument: "s << error.what() << endl;
    }
    catch(const out_of_range& error){
        cout << "Out_of_range: "s << error.what() << endl;
    }
    catch (...) {
        cout << "Unknown error"s << endl;
    }
}
