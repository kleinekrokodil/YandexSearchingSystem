#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <optional>
#include <stdexcept>

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
            if (word == "") {
                continue;
            }
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
    Document() = default;
    Document(int id_, double relevance_, int rating_)
    : id(id_), relevance(relevance_), rating(rating_) {

    }
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

//Функциональный объект для передачи в FindTopDocuments
auto key_mapper = [](const Document& document) {
    return document.id;
};

class SearchServer {
public:

    SearchServer() = default;

    explicit SearchServer(string stop_words) {
        if (!IsValidWord(stop_words)) {
            throw invalid_argument("Stop-words contain special symbols"s);
        }
        vector<string> stop_words_vector = SplitIntoWords(stop_words);
        for (const string& word : stop_words_vector) {
            stop_words_.insert(word);
        }
    }

   template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words) {
        for (const string& word : stop_words) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Stop-words contain special symbols"s);
            }
            stop_words_.insert(word);
        }
    }


    //Возврат количества документов
    size_t GetDocumentCount() const{
        return documents_.size();
    }

    //Добавление нового документа
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (!IsValidWord(document)) {
            throw invalid_argument("Document contains special symbols"s);
        }
        else if (document_id < 0 || documents_.count(document_id)) {
            throw invalid_argument("Document_id is negative or already exist"s);
        }

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

    //Создание вектора наиболее релевантных документов для вывода
    template <typename KeyMapper>
    vector<Document> FindTopDocuments(const string& query, KeyMapper key_mapper) const {
        if (!IsValidWord(query)) {
            throw invalid_argument("Query contains special symbols"s);
        }
        else if (query.find("--"s) != string::npos){
            throw invalid_argument("Query contains double-minus"s);
        }
        else if (query.find("- "s) != string::npos) {
            throw invalid_argument("No word after '-' symbol"s);
        }
        else if (query[size(query) - 1] == '-') {
            throw invalid_argument("No word after '-' symbol"s);
        }
        Query structuredQuery = ParseQuery(query);
        auto matched_documents = FindAllDocuments(structuredQuery, key_mapper);

        //Сортировка по убыванию релевантности / возрастанию рейтинга
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });

        //Усечение вывода до требуемого максимума
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    //Создание вектора наиболее релевантных документов для вывода с отсутствующим вторым аргументом либо со статусом в качестве аргумента
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus doc_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
    }

    //Метод возврата списка совпавших слов запроса
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        if (!IsValidWord(raw_query)) {
            throw invalid_argument("Query contains special symbols"s);
        }
        else if (raw_query.find("--"s) != string::npos) {
            throw invalid_argument("Query contains double-minus"s);
        }
        else if (raw_query.find("- "s) != string::npos) {
            throw invalid_argument("No word after '-' symbol"s);
        }
        else if (raw_query[size(raw_query) - 1] == '-') {
            throw invalid_argument("No word after '-' symbol"s);
        }
        Query query = ParseQuery(raw_query);
        set<string> BingoWords = {}; //Чтобы не сортировать и не проверять на совпадение

        //Обработка вектора плюс-слов
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (auto [document_, TF] : word_to_document_freqs_.at(word)) {
                //проверка совпадения по документу и запись слова в вектор
                if (document_ == document_id) {
                    BingoWords.insert(word);
                }
            }
        }

        //Исключение документов с минус-словами
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (auto [document_, TF] : word_to_document_freqs_.at(word)) {
                //если совпадение по документу - очистка вектора
                if (document_ == document_id) {
                    BingoWords.clear();
                }
            }
        }
        vector<string> v(BingoWords.begin(), BingoWords.end());
        return tuple(v, documents_.at(document_id).status);
    }

    int GetDocumentId(int index) const {   
        int i = 0;
        for (const auto& doc_ : documents_) {
            if (i == index) {
                return doc_.first;
            }
            ++i;
        }
        throw out_of_range("Index is out of range");
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_; //Список стоп-слов
    map<string, map<int, double>> word_to_document_freqs_; //Словарь "Слово" - "Документ - TF"
    map<int, DocumentData> documents_; //Словарь "Документ" - "Рейтинг - Статус"

    //Проверка входящего слова на принадлежность к стоп-словам
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    //Разбивка строки запроса на вектор слов, исключая стоп-слова
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    //Метод подсчета среднего рейтинга
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

    //Отсечение "-" у минус-слов
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

    //Создание списков плюс- и минус-слов
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

    //Вычисление IDF слова
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    //Поиск всех подходящих по запросу документов
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

        //Исключение документов с минус-словами
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        //Создание вектора вывода поискового запроса
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

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        // Возвращает true, если в слове отсутствуют спецсимволы
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

};

//Тестирующий блок
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
        SearchServer server0("in       the"s);
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
//Функция запуска теста для макроса RUN_TEST и вывода сообщения об успешном завершении теста
template <typename T>
void RunTestImpl(const T& t, const string& t_str) {
    t();
    cerr << t_str << " OK"s << endl;
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


void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

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

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    TestSearchServer();
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}