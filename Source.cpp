#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>

using namespace std;


const int MAX_RESULT_DOCUMENT_COUNT = 5;

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
            words.push_back(word);
            word = "";
        }
        else {
            word += c;
        }
    }
    words.push_back(word);

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

//�������������� ������ ��� �������� � FindTopDocuments
auto key_mapper = [](const Document& document) {
    return document.id;
};

class SearchServer {
public:
    //�������� ������ ����-����
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    //������� ���������� ����������
    size_t GetDocumentCount() {
        return documents_.size();
    }

    //���������� ������ ���������
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    //�������� ������� �������� ����������� ���������� ��� ������
    template <typename KeyMapper>
    vector<Document> FindTopDocuments(const string& query, KeyMapper key_mapper) const {
        Query structuredQuery = ParseQuery(query);
        auto matched_documents = FindAllDocuments(structuredQuery, key_mapper);

        //���������� �� �������� ������������� / ����������� ��������
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });

        //�������� ������ �� ���������� ���������
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    //�������� ������� �������� ����������� ���������� ��� ������ � ������������� ������ ���������� ���� �� �������� � �������� ���������
    vector<Document> FindTopDocuments(const string& query, DocumentStatus doc_status = DocumentStatus::ACTUAL) const {

        return FindTopDocuments(query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
    }

    //����� �������� ������ ��������� ���� �������
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        Query query = ParseQuery(raw_query);
        set<string> BingoWords = {}; //����� �� ����������� � �� ��������� �� ����������

        //��������� ������� ����-����
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (auto [document_, TF] : word_to_document_freqs_.at(word)) {
                //�������� ���������� �� ��������� � ������ ����� � ������
                if (document_ == document_id) {
                    BingoWords.insert(word);
                }
            }
        }
        ;

        //���������� ���������� � �����-�������
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (auto [document_, TF] : word_to_document_freqs_.at(word)) {
                //���� ���������� �� ��������� - ������� �������
                if (document_ == document_id) {
                    BingoWords.clear();
                }
            }
        }
        vector<string> v(BingoWords.begin(), BingoWords.end());
        return tuple(v, documents_.at(document_id).status);
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_; //������ ����-����
    map<string, map<int, double>> word_to_document_freqs_; //������� "�����" - "�������� - TF"
    map<int, DocumentData> documents_; //������� "��������" - "������� - ������"

    //�������� ��������� ����� �� �������������� � ����-������
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    //�������� ������ ������� �� ������ ����, �������� ����-�����
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    //����� �������� �������� ��������
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

    //��������� "-" � �����-����
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    //�������� ������� ����- � �����-����
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    //���������� IDF �����
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    //����� ���� ���������� �� ������� ����������
    template <typename KeyMapper>
    vector<Document> FindAllDocuments(const Query& query, KeyMapper key_mapper) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (key_mapper(documents_.find(document_id)->first, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        //���������� ���������� � �����-�������
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        //�������� ������� ������ ���������� �������
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

//������ ����������� ���� � ������� � ����������
void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

//������ ���������
void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

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
        assert(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        assert(doc0.id == doc_id);
    }

    // ����� ����������, ��� ����� ����� �� �����, ��������� � ������ ����-����,
    // ���������� ������ ���������
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        assert(server.FindTopDocuments("in"s).empty());
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
    assert(found_docs.size() == 1);
    assert(found_docs.at(0).id == 42);
    assert(found_docs.at(0).rating == 2);
}

void TestMinusWordAccounting() {
    const int doc_id = 42;
    const string doc = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.AddDocument(doc_id, doc, DocumentStatus::ACTUAL, ratings);
    //��� ������ � �����-������ ����� ������ ���� ������
    const auto found_docs = server.FindTopDocuments("cat in the -city"s);
    assert(found_docs.size() == 0);
}

void TestMatchingDocument() {
    const int doc_id = 42;
    const string doc = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.AddDocument(doc_id, doc, DocumentStatus::ACTUAL, ratings);
    //��� ������ ��� ����-����� ������ ���� ������� ������ �� ������� ��������� ���� � ������� ���������
    auto found_docs = server.MatchDocument("cat"s, doc_id);
    assert(get<vector<string>>(found_docs).at(0) == "cat"s);
    assert(get<DocumentStatus>(found_docs) == DocumentStatus::ACTUAL);
    //��� ������ � �����-������ - ������ ������
    found_docs = server.MatchDocument("cat -in"s, doc_id);
    assert(get<vector<string>>(found_docs).size() == 0);
}

void TestSortingByRelevance() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, -3 });
    server.AddDocument(43, "dog in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    //��� ������ �� ����� cat ������������� ��������� 42 ����� ����, ��� ������ �� ����� city - ����������, ���������� ����� �� �������� ��������
    auto found_docs = server.FindTopDocuments("cat"s);
    assert(found_docs[0].id == 42);
    found_docs.clear();
    found_docs = server.FindTopDocuments("city"s);
    assert(found_docs[0].id == 43);
}

void TestComputeAverageRating() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {});
    server.AddDocument(43, "dog in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    //��� ���������� ������ � �������� ������ ������������ 0, ��� ������� - ������� �������������� �������� (2)
    auto found_docs = server.FindTopDocuments("cat"s);
    assert(found_docs[0].rating == 0);
    found_docs.clear();
    found_docs = server.FindTopDocuments("dog"s);
    assert(found_docs[0].rating == 2);
}

void TestDocumentsFiltration() {
    //�������� ����������:
    //1. �� ��������� - ������ ��������� - ��������� ����
    SearchServer server;
    server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, { 9 });
    //2. �� id ��������� - ������������ ���� �������� � id == 2
    auto found_docs = server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return document_id == 2; });
    assert(found_docs[0].id == 2 && found_docs.size() == 1);
    found_docs.clear();
    //3. �� �������� - ��� �������� ������� ������ ��������� 1 � 3
    found_docs = server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return rating >= 5 ; });
    assert(found_docs[0].id == 1 && found_docs[1].id == 3 && found_docs.size() == 2);
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
    assert(found_docs[0].id == 1 && found_docs[1].id == 0 && found_docs.size() == 2);
    //����� � ��������� ������� ACTUAL - �� �� ������
    found_docs.clear();
    found_docs = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::ACTUAL);
    assert(found_docs[0].id == 1 && found_docs[1].id == 0 && found_docs.size() == 2);
    //����� � ��������� ������� (BANNED) - ������ ��������� 3
    found_docs.clear();
    found_docs = server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::BANNED);
    assert(found_docs[0].id == 3 && found_docs.size() == 1);
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
    assert(abs(found_docs[0].relevance - rel_doc1) < 1e-6);
    assert(abs(found_docs[1].relevance - rel_doc0) < 1e-6);
}

// ������� TestSearchServer �������� ������ ����� ��� ������� ������
void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestAddingDocument();
    TestMinusWordAccounting();
    TestMatchingDocument();
    TestSortingByRelevance();
    TestComputeAverageRating();
    TestDocumentsFiltration();
    TestDocumentsSearchByStatus();
    TestRelevanceCalculation();
    // �� �������� �������� ��������� ����� �����
}

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
    SearchServer search_server;
    search_server.SetStopWords("� � ��"s);

    search_server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, { 9 });

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("�������� ��������� ���"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("�������� ��������� ���"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}