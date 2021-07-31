#include "search_server.h"
#include <stdexcept>
#include <algorithm>
#include <math.h>

using namespace std;

//Возврат количества документов
size_t SearchServer::GetDocumentCount() const {
    return documents_.size();
}

//Добавление нового документа
void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
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
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id,
        DocumentData{
            ComputeAverageRating(ratings),
            status
        });
    document_ids_.insert(document_id);
}

//Метод возврата списка совпавших слов запроса
tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {

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

//Проверка входящего слова на принадлежность к стоп-словам
bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

//Разбивка строки запроса на вектор слов, исключая стоп-слова
vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

//Метод подсчета среднего рейтинга
int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

//Отсечение "-" у минус-слов
SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    bool is_minus = false;
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

//Создание списков плюс- и минус-слов
SearchServer::Query SearchServer::ParseQuery(const string& raw_query) const {
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
    Query query;
    for (const string& word : SplitIntoWords(raw_query)) {
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
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
}


bool SearchServer::IsValidWord(const string& word) {
    // A valid word must not contain special characters
    // Возвращает true, если в слове отсутствуют спецсимволы
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
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
        cout << "Error adding document "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Search results for the query: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "Search error: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Matching documents for the query: "s << query << endl;
        for (const int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Error matching documents for the query: "s << query << ": "s << e.what() << endl;
    }
}

//Метод получения частот слов по id документа
const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double> empty_words = {};
    if (document_to_word_freqs_.count(document_id)) {
        return document_to_word_freqs_.at(document_id);
    }//logN
    else {
        return empty_words;
    }      
}//logN

//Метод удаления документов из поискового сервера
void SearchServer::RemoveDocument(int document_id) {
    for (auto [word, freq] : document_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }//WlogN + 1 = WlogN
    document_to_word_freqs_.erase(document_id);//logN + 1 = logN
    document_ids_.erase(document_id);//logN + 1
    documents_.erase(document_id);//logN + 1
}//WlogN

