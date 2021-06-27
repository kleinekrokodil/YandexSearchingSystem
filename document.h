#pragma once

struct Document {
    Document();
    Document(int id_, double relevance_, int rating_);
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};