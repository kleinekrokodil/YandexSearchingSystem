#include "process_queries.h"
#include <execution>

using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const vector<string>& queries) {
    vector<vector<Document>> result(queries.size());
    transform(execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const auto& query) {return search_server.FindTopDocuments(query); });
    return result;
}