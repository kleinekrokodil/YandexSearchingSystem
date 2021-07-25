#pragma once

#include <vector>
#include <string>
#include "document.h"



void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
    const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
void TestExcludeStopWordsFromAddedDocumentContent();

void TestAddingDocument();
void TestMinusWordAccounting();
void TestMatchingDocument();
void TestSortingByRelevance();
void TestComputeAverageRating();
void TestDocumentsFiltration();
void TestDocumentsSearchByStatus();
void TestRelevanceCalculation();
//Функция запуска теста для макроса RUN_TEST и вывода сообщения об успешном завершении теста
template <typename T>
void RunTestImpl(const T& t, const std::string& t_str) {
    t();
    std::cerr << t_str << " OK" << std::endl;
}
#define RUN_TEST(func)  RunTestImpl(func, #func)
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();