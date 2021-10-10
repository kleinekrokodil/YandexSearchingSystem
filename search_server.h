#pragma once
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
#include <string>
#include <set>
#include <vector>
#include <tuple>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <execution>
#include <string_view>

extern const int MAX_RESULT_DOCUMENT_COUNT;

//Функциональный объект для передачи в FindTopDocuments
static auto key_mapper = [](const Document& document) {
    return document.id;
};


class SearchServer {
public:

    SearchServer() = default;

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words) {
        for (const auto& word : stop_words) {
            if (!IsValidWord(word)) {
                throw std::invalid_argument("Stop-words contain special symbols");
            }
            stop_words_.insert(std::string(word.data(), word.size()));
        }
    }

    explicit SearchServer(std::string stop_words)
        :SearchServer(SplitIntoWords(stop_words))
    {
    }

    explicit SearchServer(std::string_view stop_words)
        :SearchServer(SplitIntoWords(stop_words))
    {
    }

    //Методы begin и end
    std::set<int>::const_iterator begin() const{
        return document_ids_.begin();
    }

    std::set<int>::const_iterator end() const{
        return document_ids_.end();
    }

    //Возврат количества документов
    size_t GetDocumentCount() const;

    //Добавление нового документа
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    //Создание вектора наиболее релевантных документов для вывода
    template <typename KeyMapper>
    std::vector<Document> FindTopDocuments(const std::string_view query, KeyMapper key_mapper) const {
        
        Query structuredQuery = ParseQuery(query);
        auto matched_documents = FindAllDocuments(structuredQuery, key_mapper);

        //Сортировка по убыванию релевантности / возрастанию рейтинга
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });

        //Усечение вывода до требуемого максимума
        if (matched_documents.size() > unsigned(MAX_RESULT_DOCUMENT_COUNT)) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    //Создание вектора наиболее релевантных документов для вывода со статусом в качестве аргумента
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus doc_status) const {
        return SearchServer::FindTopDocuments(raw_query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
    }

    //Создание вектора наиболее релевантных документов для вывода с отсутствующим вторым аргументом 
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const {
        return SearchServer::FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    //FindTopDocuments с execution::seq
    template <typename KeyMapper>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view query, KeyMapper key_mapper) const {
        return FindTopDocuments(query, key_mapper);
    }
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentStatus doc_status) const {
        return SearchServer::FindTopDocuments(raw_query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
    }
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query) const {
        return SearchServer::FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }


    //Создание вектора наиболее релевантных документов для вывода (параллельные версии)
    template <typename KeyMapper>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view query, KeyMapper key_mapper) const {

        Query structuredQuery = ParseQuery(query);
        auto matched_documents = FindAllDocuments(std::execution::par, structuredQuery, key_mapper);

        //Сортировка по убыванию релевантности / возрастанию рейтинга
        sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });

        //Усечение вывода до требуемого максимума
        if (matched_documents.size() > unsigned(MAX_RESULT_DOCUMENT_COUNT)) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentStatus doc_status) const {
        return SearchServer::FindTopDocuments(std::execution::par, raw_query, [doc_status](int document_id, DocumentStatus status, int rating) { return status == doc_status; });
    }

    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query) const {
        return SearchServer::FindTopDocuments(std::execution::par, raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    //Метод возврата списка совпавших слов запроса
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;

    //Метод получения частот слов по id документа
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    //Метод удаления документов из поискового сервера
    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    void RemoveDocument(const std::execution::parallel_policy&, int document_id); 

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string, std::less<>> stop_words_; //Список стоп-слов
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_; //Словарь "Слово" - "Документ - TF"
    std::map<int, DocumentData> documents_; //Словарь "Документ" - "Рейтинг - Статус"
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::set<std::string, std::less<>> words_; //список слов

    //Проверка входящего слова на принадлежность к стоп-словам
    bool IsStopWord(const std::string_view word) const;

    //Разбивка строки запроса на вектор слов, исключая стоп-слова
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    //Метод подсчета среднего рейтинга
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    //Отсечение "-" у минус-слов
    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    //Создание списков плюс- и минус-слов
    Query ParseQuery(const std::string_view raw_query) const;

    //Вычисление IDF слова
    double ComputeWordInverseDocumentFreq(const std::string_view) const;

    //Поиск всех подходящих по запросу документов
    template <typename KeyMapper>
    std::vector<Document> FindAllDocuments(const Query& query, KeyMapper key_mapper) const {
        std::map<int, double> document_to_relevance;
        for (const auto word : query.plus_words) {
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
        for (const auto word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        //Создание вектора вывода поискового запроса
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }

    template <typename KeyMapper>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query, KeyMapper key_mapper) const {
        ConcurrentMap<int, double> document_to_relevance(4);
        std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), 
            [&](const auto word){
                if (word_to_document_freqs_.count(word)) {
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                    for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        if (key_mapper(documents_.find(document_id)->first, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                        }
                    }
                }   
            });

        //Исключение документов с минус-словами
        std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
            [&](const auto word) {
                if (word_to_document_freqs_.count(word)) {
                    for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        document_to_relevance.erase(document_id);
                    }
                }
            });

        //Создание вектора вывода поискового запроса
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }

    static bool IsValidWord(const std::string_view word);
};

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string_view raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string_view query);

