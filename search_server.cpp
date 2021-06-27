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
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
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

    int SearchServer::GetDocumentId(int index) const {
        int i = 0;
        for (const auto& doc_ : documents_) {
            if (i == index) {
                return doc_.first;
            }
            ++i;
        }
        throw out_of_range("Index is out of range");
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