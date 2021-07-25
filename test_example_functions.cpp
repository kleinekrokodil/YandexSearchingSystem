#include "test_example_functions.h"
#include "search_server.h"

using namespace std;

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_HINT(found_docs.size() == 1, "Something wrong with searching in TestExcludeStopWords"s);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.id == doc_id, "Something wrong with searching in TestExcludeStopWord"s);
    }
    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server0("in the"s);
        server0.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server0.FindTopDocuments("in"s).empty(), "Something wrong with excluding stop-words"s);
        vector<string> stop_words = { "in"s, "the"s };
        SearchServer server1(stop_words);
        server1.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server1.FindTopDocuments("in"s).empty(), "Something wrong with excluding stop-words"s);
    }
}

void TestAddingDocument() {
    const int doc_id = 42;
    const string doc = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.AddDocument(doc_id, doc, DocumentStatus::ACTUAL, ratings);
    //Проверяем, что документ добавился
    const auto found_docs = server.FindTopDocuments("cat in the city"s);
    ASSERT_HINT(found_docs.size() == 1, "Something wrong with adding document"s);
    ASSERT_HINT(found_docs.at(0).id == 42, "Something wrong with reading document_id"s);
    ASSERT_HINT(found_docs.at(0).rating == 2, "Something wrong with calculating average rating"s);
}
void TestMinusWordAccounting() {
    const int doc_id = 42;
    const string doc = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.AddDocument(doc_id, doc, DocumentStatus::ACTUAL, ratings);
    //При поиске с минус-словом вывод должен быть пустым
    const auto found_docs = server.FindTopDocuments("cat in the -city"s);
    ASSERT_HINT(found_docs.size() == 0, "Minus-words is not processed"s);
}
void TestMatchingDocument() {
    const int doc_id = 42;
    const string doc = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.AddDocument(doc_id, doc, DocumentStatus::ACTUAL, ratings);
    //При поиске без стоп-слова должен быть выведен кортеж из вектора совпавших слов и статуса документа
    auto found_docs = server.MatchDocument("cat"s, doc_id);
    ASSERT_HINT(get<vector<string>>(found_docs).at(0) == "cat"s, "Something wrong with matching"s);
    ASSERT_HINT(get<DocumentStatus>(found_docs) == DocumentStatus::ACTUAL, "Something wrong with matching"s);
    //При поиске с минус-словом - пустой кортеж
    found_docs = server.MatchDocument("cat -in"s, doc_id);
    ASSERT_HINT(get<vector<string>>(found_docs).size() == 0, "Matching doesn't process request with minus-words"s);
}
void TestSortingByRelevance() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, -3 });
    server.AddDocument(43, "dog in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    //При поиске по слову cat релевантность документа 42 будет выше, при поиске по слову city - одинаковая, сортировка будет по убыванию рейтинга
    auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_HINT(found_docs[0].id == 42, "Sorting isn't correct"s);
    found_docs.clear();
    found_docs = server.FindTopDocuments("city"s);
    ASSERT_HINT(found_docs[0].id == 43, "Sorting documents with equal relevance isn't correct"s);
}
void TestComputeAverageRating() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {});
    server.AddDocument(43, "dog in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    //При отсутствии данных о рейтинге должен возвращаться 0, при наличии - среднее арифметическое значений (2)
    auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_HINT(found_docs[0].rating == 0, "Documents with empty rating are processed incorrectly"s);
    found_docs.clear();
    found_docs = server.FindTopDocuments("dog"s);
    ASSERT_HINT(found_docs[0].rating == 2, "Rating is calculated incorrectly"s);
}
void TestDocumentsFiltration() {
    //Варианты фильтрации:
    //1. По умолчанию - статус документа - отдельный тест TestDocumentsSearchByStatus
    SearchServer server;
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    //2. По id документа - возвращается один документ с id == 2
    auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id == 2; });
    ASSERT_HINT(found_docs.size() == 1 && found_docs[0].id == 2, "Filtering with a predicate is wrong"s);
    found_docs.clear();
    //3. По рейтингу - при заданном запросе вернет документы 1 и 3
    found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return rating >= 5; });
    ASSERT_HINT(found_docs.size() == 2 && found_docs[0].id == 1 && found_docs[1].id == 3, "Filtering with a predicate is wrong"s);
}
void TestDocumentsSearchByStatus() {
    SearchServer server;
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::IRRELEVANT, { 5, -12, 2, 1 });
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    //Варианты:
    //Поиск по умолчанию без указания статуса - выдача актуальных документов 0 и 1
    auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_HINT(found_docs.size() == 2 && found_docs[0].id == 1 && found_docs[1].id == 0, "Filtering by default status is wrong"s);
    //Поиск с указанием статуса ACTUAL - та же выдача
    found_docs.clear();
    found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
    ASSERT_HINT(found_docs.size() == 2 && found_docs[0].id == 1 && found_docs[1].id == 0, "Filtering by status is wrong"s);
    //Поиск с указанием статуса (BANNED) - выдача документа 3
    found_docs.clear();
    found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    ASSERT_HINT(found_docs.size() == 1 && found_docs[0].id == 3, "Filtering by status is wrong"s);
}
void TestRelevanceCalculation() {
    SearchServer server;
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    //Значение релевантности не должно отличаться от вычисленного более, чем на 1e-6.
    //Вычисленная релевантность первых двух документов по данному запросу (взято из требуемого вывода в тренажере)
    const double rel_doc0 = 0.173287;
    const double rel_doc1 = 0.866434;
    auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_HINT(abs(found_docs[0].relevance - rel_doc1) < 1e-6, "Relevance is calculated incorrectly"s);
    ASSERT_HINT(abs(found_docs[1].relevance - rel_doc0) < 1e-6, "Relevance is calculated incorrectly"s);
}

#define RUN_TEST(func)  RunTestImpl(func, #func)
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddingDocument);
    RUN_TEST(TestMinusWordAccounting);
    RUN_TEST(TestMatchingDocument);
    RUN_TEST(TestSortingByRelevance);
    RUN_TEST(TestComputeAverageRating);
    RUN_TEST(TestDocumentsFiltration);
    RUN_TEST(TestDocumentsSearchByStatus);
    RUN_TEST(TestRelevanceCalculation);
    cerr << "Search server testing finished"s << endl;
}