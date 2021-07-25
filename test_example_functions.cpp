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
    // ������� ����������, ��� ����� �����, �� ��������� � ������ ����-����,
    // ������� ������ ��������
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_HINT(found_docs.size() == 1, "Something wrong with searching in TestExcludeStopWords"s);
        const Document& doc0 = found_docs[0];
        ASSERT_HINT(doc0.id == doc_id, "Something wrong with searching in TestExcludeStopWord"s);
    }
    // ����� ����������, ��� ����� ����� �� �����, ��������� � ������ ����-����,
    // ���������� ������ ���������
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
    //���������, ��� �������� ���������
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
    //��� ������ � �����-������ ����� ������ ���� ������
    const auto found_docs = server.FindTopDocuments("cat in the -city"s);
    ASSERT_HINT(found_docs.size() == 0, "Minus-words is not processed"s);
}
void TestMatchingDocument() {
    const int doc_id = 42;
    const string doc = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.AddDocument(doc_id, doc, DocumentStatus::ACTUAL, ratings);
    //��� ������ ��� ����-����� ������ ���� ������� ������ �� ������� ��������� ���� � ������� ���������
    auto found_docs = server.MatchDocument("cat"s, doc_id);
    ASSERT_HINT(get<vector<string>>(found_docs).at(0) == "cat"s, "Something wrong with matching"s);
    ASSERT_HINT(get<DocumentStatus>(found_docs) == DocumentStatus::ACTUAL, "Something wrong with matching"s);
    //��� ������ � �����-������ - ������ ������
    found_docs = server.MatchDocument("cat -in"s, doc_id);
    ASSERT_HINT(get<vector<string>>(found_docs).size() == 0, "Matching doesn't process request with minus-words"s);
}
void TestSortingByRelevance() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, -3 });
    server.AddDocument(43, "dog in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    //��� ������ �� ����� cat ������������� ��������� 42 ����� ����, ��� ������ �� ����� city - ����������, ���������� ����� �� �������� ��������
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
    //��� ���������� ������ � �������� ������ ������������ 0, ��� ������� - ������� �������������� �������� (2)
    auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_HINT(found_docs[0].rating == 0, "Documents with empty rating are processed incorrectly"s);
    found_docs.clear();
    found_docs = server.FindTopDocuments("dog"s);
    ASSERT_HINT(found_docs[0].rating == 2, "Rating is calculated incorrectly"s);
}
void TestDocumentsFiltration() {
    //�������� ����������:
    //1. �� ��������� - ������ ��������� - ��������� ���� TestDocumentsSearchByStatus
    SearchServer server;
    server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, { 9 });
    //2. �� id ��������� - ������������ ���� �������� � id == 2
    auto found_docs = server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return document_id == 2; });
    ASSERT_HINT(found_docs.size() == 1 && found_docs[0].id == 2, "Filtering with a predicate is wrong"s);
    found_docs.clear();
    //3. �� �������� - ��� �������� ������� ������ ��������� 1 � 3
    found_docs = server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return rating >= 5; });
    ASSERT_HINT(found_docs.size() == 2 && found_docs[0].id == 1 && found_docs[1].id == 3, "Filtering with a predicate is wrong"s);
}
void TestDocumentsSearchByStatus() {
    SearchServer server;
    server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::IRRELEVANT, { 5, -12, 2, 1 });
    server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, { 9 });
    //��������:
    //����� �� ��������� ��� �������� ������� - ������ ���������� ���������� 0 � 1
    auto found_docs = server.FindTopDocuments("�������� ��������� ���"s);
    ASSERT_HINT(found_docs.size() == 2 && found_docs[0].id == 1 && found_docs[1].id == 0, "Filtering by default status is wrong"s);
    //����� � ��������� ������� ACTUAL - �� �� ������
    found_docs.clear();
    found_docs = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::ACTUAL);
    ASSERT_HINT(found_docs.size() == 2 && found_docs[0].id == 1 && found_docs[1].id == 0, "Filtering by status is wrong"s);
    //����� � ��������� ������� (BANNED) - ������ ��������� 3
    found_docs.clear();
    found_docs = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::BANNED);
    ASSERT_HINT(found_docs.size() == 1 && found_docs[0].id == 3, "Filtering by status is wrong"s);
}
void TestRelevanceCalculation() {
    SearchServer server;
    server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, { 9 });
    //�������� ������������� �� ������ ���������� �� ������������ �����, ��� �� 1e-6.
    //����������� ������������� ������ ���� ���������� �� ������� ������� (����� �� ���������� ������ � ���������)
    const double rel_doc0 = 0.173287;
    const double rel_doc1 = 0.866434;
    auto found_docs = server.FindTopDocuments("�������� ��������� ���"s);
    ASSERT_HINT(abs(found_docs[0].relevance - rel_doc1) < 1e-6, "Relevance is calculated incorrectly"s);
    ASSERT_HINT(abs(found_docs[1].relevance - rel_doc0) < 1e-6, "Relevance is calculated incorrectly"s);
}

#define RUN_TEST(func)  RunTestImpl(func, #func)
// ������� TestSearchServer �������� ������ ����� ��� ������� ������
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