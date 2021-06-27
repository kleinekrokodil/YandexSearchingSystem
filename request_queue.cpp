#include "request_queue.h"


RequestQueue::RequestQueue(const SearchServer& search_server)
    :search_server_(search_server)
{}

    int RequestQueue::GetNoResultRequests() const {
        return RequestQueue::no_res_requests;
    }