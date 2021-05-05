/*Учебный проект поисковой системы
* Создатель Кибирев В.Ю. */

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cmath>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5; //Максимальное число выводимых результатов

string ReadLine() {
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() {
	int result;
	cin >> result;
	ReadLine();
	return result;
}

//Разбивка строки в вектор
vector<string> SplitIntoWords(const string& text) {
	vector<string> words;
	string word;
	for (const char c : text) {
		if (c == ' ') {
			words.push_back(word);
			word = "";
		}
		else {
			word += c;
		}
	}
	words.push_back(word);

	return words;
}

struct Document {
	int id;
	double relevance;
};

struct Query
{
	vector<string> plusWords;
	vector<string> minusWords;
};

class SearchServer {
public:
	//Создание множества стоп-слов
	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	//Создание словаря слов запроса с параметром term frequency или TF
	void AddDocument(int document_id, const string& document) {

		documents_length_[document_id] = SplitIntoWordsNoStop(document).size();

		for (const string& word : SplitIntoWordsNoStop(document)) {
			double word_count = 1.0;
			if (word_to_documents_freqs_[word].count(document_id)) {
				++word_count;
				word_to_documents_freqs_[word].erase(document_id);
			}
			word_to_documents_freqs_[word].insert({ document_id, word_count / SplitIntoWordsNoStop(document).size() });
			//cout << word << ' ' << document_id << ' ' << word_count / SplitIntoWordsNoStop(document).size() << endl;
		}

	}

	//Создание вектора наиболее релевантных документов для вывода
	vector<Document> FindTopDocuments(const string& query) const {
		Query structuredQuery = ParseQuery(query);
		auto matched_documents = FindAllDocuments(structuredQuery);

		sort(
			matched_documents.begin(),
			matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				return lhs.relevance > rhs.relevance;
			}
		);
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

private:
	map<string, map<int, double>> word_to_documents_freqs_; //словарь "Слово - Документ - TF"
	set<string> stop_words_;
	map<int, int> documents_length_;

	//Разбивка документа с исключением стоп-слов
	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (stop_words_.count(word) == 0) {
				words.push_back(word);
			}
		}
		return words;
	}

	//Создание структуры плюс-минус слов
	Query ParseQuery(const string& query) const {
		const vector<string> query_words = SplitIntoWordsNoStop(query);
		Query structuredQuery;
		for (const string& word : query_words) {
			if (word[0] == '-') {
				structuredQuery.minusWords.push_back(word.substr(1));
			}
			else structuredQuery.plusWords.push_back(word);
		}
		return structuredQuery;
	}

	//Обработка поискового запроса
	vector<Document> FindAllDocuments(const Query& query) const {
		map<int, double> document_to_relevance;
		/* Вывод текущих значений векторов для проверки
		for (auto [document_id, size] : documents_length_) {
			cout << "Document " << document_id << " size: " << size << endl;
		}
		for (auto [word, value] : word_to_documents_freqs_) {
			cout << word << endl;
			for (auto [doc, tf] : word_to_documents_freqs_.at(word)) {
				cout << '{' << doc << ", " << tf << '}' << endl;
			}
		}
		cout << "documents_length_.size = " << documents_length_.size() << endl;*/
		//Обработка вектора плюс-слов
		for (const string& word : query.plusWords) {
			//cout << '{' << word << '}' << endl;
			if (word_to_documents_freqs_.count(word) == 0) {
				continue;
			}
			for (auto [document_id, TF] : word_to_documents_freqs_.at(word)) {
				//cout << "doc {" << document_id << ' ' << documents_length_.size() << ' ' << word_to_documents_freqs_.at(word).size()  << '}' << endl;

				double IDF = log(static_cast<double>(documents_length_.size()) / word_to_documents_freqs_.at(word).size());

				//cout <<word << " : " << TF << " : " << word_to_documents_freqs_.at(word).size() << " : " << IDF << endl;
				document_to_relevance[document_id] += TF * IDF;
			}
		}

		//Исключение документов с минус-словами
		for (const string& word : query.minusWords) {
			if (word_to_documents_freqs_.count(word) == 0) {
				continue;
			}
			for (auto [document_id, TF] : word_to_documents_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		//Вектор результатов поискового запроса
		vector<Document> matched_documents;
		for (auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({ document_id, relevance });
		}

		return matched_documents;
	}
};

//Собственно поисковая функция
SearchServer CreateSearchServer() {
	SearchServer search_server;
	search_server.SetStopWords(ReadLine());

	const int document_count = ReadLineWithNumber();

	for (int document_id = 0; document_id < document_count; ++document_id) {
		search_server.AddDocument(document_id, ReadLine());
	}

	return search_server;
}


int main() {
	const SearchServer search_server = CreateSearchServer();

	const string query = ReadLine();
	for (auto [document_id, relevance] : search_server.FindTopDocuments(query)) {
		cout << "{ document_id = " << document_id << ", relevance = " << relevance << " }" << endl;
	}
}