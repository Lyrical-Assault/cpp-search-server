#include "test_example_functions.h"

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint) {
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent(){
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server(""s);
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
    const std::vector<std::string_view> test_words = {"black"s, "dog"s};
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
    const std::vector<int> ratings = {1, 2, 3};
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
    const std::string raw_query = "fluffy cat"s;
    const std::string content1 = "white cat fluffy tail"s;
    const std::string content2 = "black dog beautiful eyes"s;
    server.AddDocument(doc0_id, content1, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(doc1_id, content2, DocumentStatus::ACTUAL, {3, 4});
    const auto found_docs = server.FindTopDocuments(raw_query);
    ASSERT_EQUAL(static_cast<int>(found_docs.size()), 1);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL_HINT(doc0.id, doc0_id, "Wrong document id!"s);
    ASSERT_HINT(std::abs(doc0.relevance - (log(server.GetDocumentCount() * 1.0 / 1) * (2.0 / 4))) < EPSILON, "Relevance is compute incorrectly!"s);
}
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer(){
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