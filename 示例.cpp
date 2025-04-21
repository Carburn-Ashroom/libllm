#include <iostream>

#include <llm.hpp>

using std::cout;
using std::string;
using std::cin;
using std::cerr;

void read(string&& word);          // 通用模型的回调函数
void think_read(string&& word, bool reasoning);        // 深度思考模型的回调函数

int main()
{
    LLM::R1 llm {"你的API key",think_read};
    for ( ; ; ) {
        string line;
        std::getline(cin, line);
        try {
            llm.get(line);
        }
        catch (Curl::Network_error) {
            cerr << "无法连接网络，请检查网络连接后重试";
        }
        catch (const LLM::LLM_error& err) {
            cerr << err.what();
        }
        cout << "\n\n";
    }
    return 0;
}

void read(string&& word)
{
    cout << word;
}

void think_read(string&& word, bool reasoning)
{
    static bool think {};
    if (reasoning != think) {
        cout << ((reasoning) ? "\n深度思考：" : "\n\n实际输出：") << '\n';
        think = reasoning;
    }
    cout << word;
}
