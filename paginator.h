#pragma once
#include <iostream>
#include <stdexcept>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator& page_begin, Iterator& page_end) :
        page_begin_(page_begin),
        page_end_(page_end)
    {
    }
    Iterator begin() const {
        return page_begin_;
    }
    Iterator end() const {
        return page_end_;
    }
    size_t size() const {
        return distance(page_begin_, page_end_);
    }
private:
    Iterator page_begin_;
    Iterator page_end_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, const int& page_size) {
        if (page_size < 1) {
            throw std::invalid_argument("Page size has negative or zero value");
        }
        auto doc_size = distance(begin, end);
        if (doc_size % page_size == 0) {
            size_ = doc_size / page_size;
        }
        else {
            size_ = doc_size / page_size + 1;
        }
        auto page_begin = begin;
        auto page_end = begin;
        for (size_t i = 0; i < size_; ++i) {
            if (i == size_ - 1) {
                page_end = end;
            }
            else {
                advance(page_end, page_size);
            }
            IteratorRange page(page_begin, page_end);
            result_.push_back(page);
            if (i == size_ - 1) {
                continue;
            }
            else {
                advance(page_begin, page_size);
            }
        }
    }
    auto begin() const {
        return result_.begin();
    }
    auto end() const {
        return result_.end();
    }
    size_t size() const {
        return size_;
    }

private:
    std::vector<IteratorRange<Iterator>> result_;
    size_t size_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

std::ostream& operator<<(std::ostream& output, const Document& doc) {
    output << "{ document_id = " << doc.id << ", relevance = " << doc.relevance << ", rating = " << doc.rating << " }";
    return output;
}

template <typename Iterator>
std::ostream& operator<<(std::ostream& output, const IteratorRange<Iterator>& range) {
    for (auto it = range.begin(); it != range.end(); ++it) {
        output << *it;
    }
    return output;
}