#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <iostream>

#include "llm.h"

using std::cout;
using std::string;
using std::cin;
using std::cerr;

void rread(string&& word, bool reasoning);
void read(string&& word);

// deepseek: sk-9c5c119f6b08445cb0d23d1bc44e3a30
// 智谱：9c751084b473726266290363c36c4da8.rWS0KvhW12gRYh99
// 阿里云：sk-8df7aa314f6f48228e095891855bd133
// 豆包：d928e571-aa07-46d9-bf4f-d9c5c3d8b4f5

int main()
{
    LLM::Polite llm {"sk-9c5c119f6b08445cb0d23d1bc44e3a30",read};
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


void rread(string&& word, bool reasoning)
{
    static bool ring {};
    if (reasoning != ring) {
        cout << ((reasoning) ? "\n深度思考：" : "\n\n实际输出：") << '\n';
        ring = reasoning;
    }
    cout << word;
}

void read(string&& word)
{
    cout << word;
}


