#pragma once

#include <vector>
#include <string>
#include <deque>
#include "search_server.h"
#include "document.h"


class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector<Document> matched_documents = search_server_.FindTopDocuments(raw_query, document_predicate);
        if (requests_.size() == sec_in_day_) {
            if (requests_.back().result_requests_ == false) {
                --no_res_requests_;
            }
            requests_.pop_back();
        }
        if (matched_documents.empty()) {
            ++no_res_requests_;
        }
        requests_.push_front({ raw_query, !matched_documents.empty() });
        return matched_documents;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status) {
        return RequestQueue::AddFindRequest(raw_query, [status](int document_id, DocumentStatus doc_status, int rating) { return status == doc_status; });
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query) {
        return RequestQueue::AddFindRequest(raw_query, [](int document_id, DocumentStatus doc_status, int rating) { return doc_status == DocumentStatus::ACTUAL; });
    }

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        const std::string raw_query_;
        bool result_requests_;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& search_server_;
    int no_res_requests_ = 0;
};