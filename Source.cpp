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

    // Defines an invalid document id
    // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

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
        ;

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