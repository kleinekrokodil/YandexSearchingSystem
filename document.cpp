#include "document.h"
#include <iostream>
#include <string>

using namespace std;

Document::Document() = default;
Document::Document(int id_, double relevance_, int rating_)
    : id(id_), relevance(relevance_), rating(rating_) {

}

ostream& operator<<(ostream& output, const Document& doc) {
    output << "{ document_id = "s << doc.id << ", relevance = "s << doc.relevance << ", rating = "s << doc.rating << " }"s;
    return output;
}