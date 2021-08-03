#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server)
    :search_server_(search_server)
{}

int RequestQueue::GetNoResultRequests() const {
    return no_res_requests_;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    return RequestQueue::AddFindRequest(raw_query, [status](int document_id, DocumentStatus doc_status, int rating) { return status == doc_status; });
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return RequestQueue::AddFindRequest(raw_query, [](int document_id, DocumentStatus doc_status, int rating) { return doc_status == DocumentStatus::ACTUAL; });
}