/*������� ������ ��������� �������
* ��������� ������� �.�. */

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
#include <numeric>
#include <tuple>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5; //������������ ����� ��������� �����������

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

vector<int> ReadVector() {
	vector<int> v;
	int size, n;
	cin >> size;
	for (int i = 0; i < size; ++i) {
		cin >> n;
		v.push_back(n);
	}
	return v;
}

//�������� ������ � ������
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

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED
};

struct Document {
	int id;
	double relevance;
	int rating;
};

class SearchServer {
public:
	//�������� ��������� ����-����
	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	size_t GetDocumentCount() {
		return documents_length_.size();
	}

	//�������� ������� ���� ������� � ���������� term frequency ��� TF
	void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int> rating) {

		documents_length_[document_id] = SplitIntoWordsNoStop(document).size();

		for (const string& word : SplitIntoWordsNoStop(document)) {
			double word_count = 1.0;
			if (word_to_documents_freqs_[word].count(document_id)) {
				++word_count;
				word_to_documents_freqs_[word].erase(document_id);
			}
			word_to_documents_freqs_[word].insert({ document_id, word_count / SplitIntoWordsNoStop(document).size() });
			
			
			documents_rating_.emplace(document_id, ComputeAverageRating(rating));
			documents_status_.emplace(document_id, status);
		}

	}

	//�������� ������� �������� ����������� ���������� ��� ������
	vector<Document> FindTopDocuments(const string& query, DocumentStatus status) const {
		Query structuredQuery = ParseQuery(query);
		auto matched_documents = FindAllDocuments(structuredQuery);

		sort(
			matched_documents.begin(),
			matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				return lhs.relevance > rhs.relevance;
			}
		);
		for (size_t i = 0; i < matched_documents.size();) {
			int id = matched_documents.at(i).id;
			if (documents_status_.at(id) != status) {
				matched_documents.erase(matched_documents.begin() + i);
			}
			else ++i;
		}

		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

	//����� �������� ������ ��������� ���� �������
	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
		Query query = ParseQuery(raw_query);
		set<string> BingoWords = {}; //����� �� ����������� � �� ��������� �� ����������
		
		//��������� ������� ����-����
		for (const string& word : query.plusWords) {
			if (word_to_documents_freqs_.count(word) == 0) {
				continue;
			}
			for (auto [document_, TF] : word_to_documents_freqs_.at(word)) {
				//�������� ���������� �� ��������� � ������ ����� � ������
				if (document_ == document_id) {
					BingoWords.insert(word);
				}
			}
		}
		;

		//���������� ���������� � �����-�������
		for (const string& word : query.minusWords) {
			if (word_to_documents_freqs_.count(word) == 0) {
				continue;
			}
			for (auto [document_, TF] : word_to_documents_freqs_.at(word)) {
				//���� ���������� �� ��������� - ������� �������
				if (document_ == document_id) {
					BingoWords.clear();
				}
			}
		}
		vector<string> v(BingoWords.begin(), BingoWords.end());
		return tuple(v, documents_status_.at(document_id));
	}

private:
	map<string, map<int, double>> word_to_documents_freqs_; //������� "����� - �������� - TF"
	set<string> stop_words_;
	map<int, int> documents_length_;
	map<int, int> documents_rating_;
	map<int, DocumentStatus> documents_status_;

	struct Query {
		vector<string> plusWords;
		vector<string> minusWords;
	};


	//�������� ��������� � ����������� ����-����
	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (stop_words_.count(word) == 0) {
				words.push_back(word);
			}
		}
		return words;
	}

	//�������� ��������� ����-����� ����
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

	//��������� ���������� �������
	vector<Document> FindAllDocuments(const Query& query) const {
		map<int, double> document_to_relevance;
		
		//��������� ������� ����-����
		for (const string& word : query.plusWords) {
			if (word_to_documents_freqs_.count(word) == 0) {
				continue;
			}
			for (auto [document_id, TF] : word_to_documents_freqs_.at(word)) {
				double IDF = log(static_cast<double>(documents_length_.size()) / word_to_documents_freqs_.at(word).size());
				document_to_relevance[document_id] += TF * IDF;
			}
		}

		//���������� ���������� � �����-�������
		for (const string& word : query.minusWords) {
			if (word_to_documents_freqs_.count(word) == 0) {
				continue;
			}
			for (auto [document_id, TF] : word_to_documents_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		//������ ����������� ���������� �������
		vector<Document> matched_documents;
		for (auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({ document_id, relevance, documents_rating_.at(document_id) });
		}

		return matched_documents;
	}

	//����� �������� �������� �������� ���������
	static int ComputeAverageRating(const vector<int>& rating) {
		int rating_count = rating.size();
		int average_rating;
		if (rating_count > 0) {
			average_rating = accumulate(rating.begin(), rating.end(), 0) / rating_count;
		}
		else average_rating = 0;
		return average_rating;
	}

	
};

//���������� ��������� �������
SearchServer CreateSearchServer() {
	SearchServer search_server;
	search_server.SetStopWords(ReadLine());

	const int document_count = ReadLineWithNumber();
	DocumentStatus status = DocumentStatus::ACTUAL;

	for (int document_id = 0; document_id < document_count; ++document_id) {
		search_server.AddDocument(document_id, ReadLine(), status, ReadVector());
		//cin.ignore();
	}

	return search_server;
}

//������ ���������
void PrintDocument(const Document& document) {
	cout << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating
		<< " }"s << endl;
}

//������ ��������� � ��������� ����
void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
	cout << "{ "s
		<< "document_id = "s << document_id << ", "s
		<< "status = "s << static_cast<int>(status) << ", "s
		<< "words ="s;
	for (const string& word : words) {
		cout << ' ' << word;
	}
	cout << "}"s << endl;
}

int main() {
	SearchServer search_server;
	search_server.SetStopWords("� � ��"s);

	search_server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	search_server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, { 9 });

	const int document_count = search_server.GetDocumentCount();
	for (int document_id = 0; document_id < document_count; ++document_id) {
		const auto [words, status] = search_server.MatchDocument("�������� ���"s, document_id);
		PrintMatchDocumentResult(document_id, words, status);
	}
}