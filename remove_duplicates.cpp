#include "remove_duplicates.h"

using namespace std;

//Поиск и удаление дубликатов
void RemoveDuplicates(SearchServer& search_server) {
    set<int> duplicates;
    for (auto i = search_server.begin(); i != search_server.end(); ++i) {
        auto doc_words1 = search_server.GetWordFrequencies(*i);
        for (auto j = i; j != search_server.end(); ++j) {
            if (i == j) {
                continue;
            }
            auto doc_words2 = search_server.GetWordFrequencies(*j);
            if (doc_words1.size() == doc_words2.size() &&
                equal(doc_words1.begin(), doc_words1.end(), doc_words2.begin(), [](pair<string, double> p1, pair<string, double> p2) {
                    return p1.first == p2.first;
                    })) {
                duplicates.insert(*j);
            }
        }
    }
    for (auto id : duplicates) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}