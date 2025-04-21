#ifndef LLM_DEPENDENCE_HPP
#define LLM_DEPENDENCE_HPP

#include <string>
#include <utility>
#include <vector>
#include <string_view>
#include <sstream>
#include <memory>
#include <fstream>
#include <complex>
#include <functional>

#include <winsock2.h>
#include <windows.h>
#include <curl.hpp>

namespace LLM_impl {             // 该名字空间负责实现大模型的基类
    
    using std::string;
    using std::vector;
    using std::string_view;
    using std::ostringstream;
    using std::shared_ptr;
    using std::istringstream;
    using std::ifstream;
    using std::ofstream;
    using std::complex;
    using std::function;
    
    enum class Mode { system, user, assistant, none };       // 文件内容分区
    
    struct Not_found_error {};       // 未找到
    struct File_format_error {};             // 文件格式错误，比如user后不是assistant，或assistant前不是user
    struct Empty_history_error {};       // 在空历史记录中寻找历史记录的异常
    
    class LLM_error {          // 生成出错
    public:
        explicit LLM_error(string&& message) : message{std::move(message)} { }
        const string& what() const { return message; }
    private:
        string message;
    };
    
    class Message_func {             // Message_func类是用户提供的回调函数和本次LLM生成的结果的绑定
    public:
        explicit Message_func(int prog_encode) : prog_encode{prog_encode} { }
        virtual void operator()(string&& ans) = 0;       // 调用回调函数处理LLM生成的token，并记录该token。ans是流式调用中每次由LLM生成的token
        string&& get_ans() { return std::move(answer); }
        int prog_enc() const { return prog_encode; }
        virtual ~Message_func() { }
    protected:
        void add_ans(string_view ans) { answer.append(ans); }
    private:
        string answer;
        int prog_encode;
    };
    
    class Reasonal_message : public Message_func {       // Reasonal_message类是用户提供的深度思考回调函数和本次LLM生成的结果的绑定，深度思考结果与答案结果保存在不同地方
    public:
        Reasonal_message(int prog_encode, function<void(string&&, bool)> func) : Message_func{prog_encode}, func{func} { }
        void reason(string&& r)        // 调用回调函数处理深度思考token，并记录该token
        {
            reasoning.append(r);
            func(std::move(r), true);
        }
        string&& remember_reasoning() { return std::move(reasoning); }
        void operator()(string&& ans) override       // 调用回调函数处理LLM生成的token，并记录该token
        {
            add_ans(ans);
            func(std::move(ans), false);
        }
    private:
        function<void(string&&, bool)> func;
        string reasoning;
    };
    
    class Chat_message : public Message_func {       // Chat_message类是回调函数与生成结果的绑定
    public:
        Chat_message(int prog_encode, function<void(string&&)> func) : Message_func{prog_encode}, func{func} { }
        void operator()(string&& ans) override       // 调用回调函数处理LLM生成的token，并记录该token
        {
            add_ans(ans);
            func(std::move(ans));
        }
    private:
        function<void(string&&)> func;
    };
    
    class LLM {        // LLM类是一个对话模型
    public:
        LLM(string&& url, string&& model, string&& key, int code_encode, int prog_encode) : url{std::move(url)}, model{std::move(model)}, key{std::move(key)}, code_encode{code_encode}, prog_encode{prog_encode}, temperature{-1} { }
        void read_file(const string& file, int file_encode =CP_UTF8)             // 从文件中读取对话历史，若文件不存在就抛出Not_found_error异常，若文件格式不对抛出File_format_error异常
        {
            ifstream f_str {file};
            if (not f_str.is_open())
                throw Not_found_error{};
            sys.clear();
            history.clear();
            string line;
            Mode mode {Mode::none};
            for (std::getline(f_str, line); not (line.empty() and f_str.eof()); std::getline(f_str, line)) {
                if (line == "system") {
                    if (mode == Mode::user)
                        throw File_format_error{};
                    mode = Mode::system;
                    sys.clear();
                }
                else if (line == "user") {
                    if (mode == Mode::user)
                        throw File_format_error{};
                    mode = Mode::user;
                    history.emplace_back();
                }
                else if (line == "assistant") {
                    if (mode != Mode::user)
                        throw File_format_error{};
                    mode = Mode::assistant;
                    history.emplace_back();
                }
                else {
                    line = encode(file_encode, prog_encode, line.c_str());
                    switch (mode) {
                    case Mode::system:
                        if (not sys.empty())
                            sys.push_back('\n');
                        sys.append(line);
                        break;
                    case Mode::user:
                    case Mode::assistant:
                        if (not history.back().empty())
                            history.back().push_back('\n');
                        history.back().append(line);
                        break;
                    default:
                        throw File_format_error{};
                    }
                }
                line.clear();
            }
            if (mode == Mode::user)
                throw File_format_error{};
        }
        bool save_file(const string& file, int file_encode =CP_UTF8)             // 保存对话历史到文件中，返回是否保存成功
        {
            ofstream f_str {file};
            if (not f_str.is_open())
                return false;
            if (not sys.empty()) {
                f_str << "system\n";
                f_str << encode(prog_encode, file_encode, sys.c_str()) << '\n';
            }
            for (auto i=history.begin(); i!=history.end(); ++i) {
                f_str << "user\n" << encode(prog_encode, file_encode, i->c_str()) << '\n';
                f_str << "assistant\n" << encode(prog_encode, file_encode, (++i)->c_str()) << '\n';
            }
            return true;
        }
        void set_system(string&& system) { sys = std::move(system); }
        virtual void get(string&& question) = 0;             // 调用大模型
        void get(const string& question) { get(string{question}); }
        void add_history(string&& ques, string&& ans)          // 设置历史记录，可以用于训练模型
        {
            history.push_back(std::move(ques));
            history.push_back(std::move(ans));
        }
        const string& get_history(string_view ques ="") const          // 获取历史记录中某问题的答案，若参数为空字符串则最近一次问题的答案，若未找到则抛出Not_found_error，若历史记录为空则抛出Empty_history_error
        {
            if (history.empty())
                throw Empty_history_error{};
            else if (ques.empty())
                return history.back();
            for (auto i=history.begin(); i!=history.end(); i+=2)
                if (*i == ques)
                    return *++i;
            throw Not_found_error{};
        }
        complex<string> get_history(int index) const             // 获取第index次对话的历史记录，若未找到则抛出Not_found_error，若历史记录为空则抛出Empty_history_error
        {
            if (history.empty())
                throw Empty_history_error{};
            unsigned locat {static_cast<unsigned>(index*2+1)};
            return (locat<history.size()) ? complex<string>{history[locat-1],history[locat]} : throw Not_found_error{};
        }
        void clear_history() { history.clear(); }
        void set_temperature(double temp) { temperature = temp; }
        void set_model(string&& m) { model = std::move(m); }
        void set(string&& property, string&& value, bool quote_value =false)             // 设定模型的某个调用参数，如果quote_value为true，就用引号括住value。
        {
            if (property == "system")
                sys = std::move(value);
            else if (property == "temperature")
                temperature = std::stod(value);
            else if (property == "model")
                model = std::move(value);
            else if (property == "url")
                url = std::move(value);
            else if (property == "key")
                key = std::move(value);
            else if (property == "stream") {
                if (value != "true")
                    throw LLM_error{"目前暂不支持非流式调用"};
            }
            else {
                value = escape(value);
                if (quote_value)
                    quote(value);
                for (auto i=settings.begin(); i!=settings.end(); i+=2)
                    if (*i == property) {
                        *++i = std::move(value);
                        return;
                    }
                settings.push_back(std::move(property));
                settings.push_back(std::move(value));
            }
        }
        static string encode(int from, int to, const char* source)       // 将source从from编码转为to编码。source不能为空指针。这段代码是deepseek写的，我也不清楚
        {
            if (from == to)
                return string{source};
            int wlen {MultiByteToWideChar(from, 0, source, -1, nullptr, 0)};
            shared_ptr<wchar_t[]> wbuf {new wchar_t[wlen]};
            MultiByteToWideChar(from, 0, source, -1, wbuf.get(), wlen);
            int glen {WideCharToMultiByte(to, 0, wbuf.get(), -1, nullptr, 0, nullptr, nullptr)};
            shared_ptr<char[]> gbuf {new char[glen]};
            WideCharToMultiByte(to, 0, wbuf.get(), -1, gbuf.get(), glen, nullptr, nullptr);
            return string{gbuf.get()};
        }
        string encode(const char* source) const { return encode(code_encode, prog_encode, source); }
        virtual ~LLM() { }
    protected:
        static string read_key(string& json, string_view key)          // 检查json字符串是否没有异常，若无异常则在json字符串中读取对应键的内容（不转义），若有异常则抛出。json是流式调用LLM时生成的json结果
        {
            if (json.find(R"("error")")!=string::npos or json.find("Failed")==0)
                throw LLM_error{std::move(json)};
            string quote_key {key};
            quote(quote_key);
            auto index = json.find(quote_key);
            if (index == string::npos)
                throw Not_found_error{};
            string result;
            bool in {};
            bool escape_mode {};
            for (auto start=index+quote_key.length(); start!=json.length(); ++start) {
                switch (json[start]) {
                case '\\':
                    escape_mode = not escape_mode;
                    break;
                case '"':
                    if (not escape_mode)
                        in = not in;
                    else
                        escape_mode = false;
                    break;
                case ' ':
                case ':':
                    if (not in)
                        continue;
                    break;
                case ',':
                case '}':
                    if (not in)
                        return result;
                    break;
                default:
                    escape_mode = false;
                    break;
                }
                result.push_back(json[start]);
            }
            return result;
        }
        Curl::Curl set_curl(string_view question) const        // 生成本次调用所需的curl对象
        {
            Curl::Curl curl {url};
            curl.add_header("Content-Type", "application/json");
            curl.add_header("Authorization", string{"Bearer "}+key);
            curl.set_body(encode(prog_encode, CP_UTF8, request_body(question).c_str()));
            return curl;
        }
        int prog_enc() const { return prog_encode; }
        int code_enc() const { return code_encode; }
        static void del_quote(string& quoted) { quoted = quoted.substr(1, quoted.length()-2); }
        static void quote(string& will_quote) { will_quote = string{'"'}+will_quote+'"'; }
        auto func_call_back() const { return call_back_func; }
        void set_call_back(function<size_t(char*, size_t, size_t, Message_func*)> func) { call_back_func = func; }
        static string parse(string_view str)             // 解析转义字符
        {
            string res;
            for (auto i=str.begin(); i!=str.end(); ++i)
                if (*i == '\\')
                    switch (*++i) {
                    case 'n':
                        res.push_back('\n');
                        break;
                    case '\\':
                        res.push_back('\\');
                        break;
                    case '"':
                        res.push_back('"');
                        break;
                    default:
                        res.push_back('\\');
                        --i;
                        break;
                    }
                else
                    res.push_back(*i);
            return res;
        }
        static string escape(string_view str)          // 添加转义字符
        {
            string res;
            for (auto ch : str)
                switch (ch) {
                case '\n':
                    res.append(R"(\n)");
                    break;
                case '\\':
                    res.append(R"(\\)");
                    break;
                case '"':
                    res.append(R"(\")");
                    break;
                default:
                    res.push_back(ch);
                    break;
                }
            return res;
        }
    private:
        string url;
        string model;
        string key;
        string sys;
        vector<string> history;
        vector<string> settings;
        int code_encode;
        int prog_encode;
        string request_body(string_view question) const        // 请求体
        {
            ostringstream ostr;
            ostr << '{';
            ostr << R"("model": ")" << model << R"(",)";
            if (temperature>=0 and temperature<=2)
                ostr << R"("temperature": )" << temperature << ',';
            for (auto i=settings.begin(); i!=settings.end(); ++i) {
                ostr << '"' << *i << R"(": )";
                ostr << *++i << ',';
            }
            ostr << R"("stream": true,)";
            ostr << R"("messages": [)";
            if (not sys.empty())
                ostr << R"({"role": "system", "content": ")" << escape(sys) << R"("},)";
            for (auto i=history.begin(); i!=history.end(); ++i) {
                ostr << R"({"role": "user", "content": ")" << escape(*i) << R"("},)";
                ostr << R"({"role": "assistant", "content": ")" << escape(*++i) << R"("},)";
            }
            ostr << R"({"role": "user", "content": ")" << escape(question) << R"("})";
            ostr << "]}";
            return ostr.str();
        }
        double temperature;
        function<size_t(char*, size_t, size_t, Message_func*)> call_back_func;
    };
    
    class Reasoner : public LLM {          // Reasoner类是深度思考模型
    public:
        Reasoner(string&& url, string&& model, string&& key, function<void(string&&, bool)> func, int code_encode, int prog_encode) : LLM{std::move(url),std::move(model),std::move(key),code_encode,prog_encode}, func{func} { set_call_back(call_back); }
        void get(string&& question) override             // 调用大模型
        {
            Curl::Curl curl {set_curl(question)};
            curl.set_write_func(reinterpret_cast<void*>(*(func_call_back().target<size_t(*)(char*, size_t, size_t, Message_func*)>())));
            Reasonal_message mfunc {prog_enc(),func};
            curl.set_write_data(&mfunc);
            curl.perform();
            add_history(std::move(question), mfunc.get_ans());
            last_reason = mfunc.remember_reasoning();
        }
        using LLM::get;
        const string& remem_reasoning() const { return last_reason; }
        virtual ~Reasoner() { }
    private:
        function<void(string&&, bool)> func;
        string last_reason;
        static size_t call_back(char* contents, size_t size, size_t nmemb, Message_func* ptr)           // 处理网络请求中每次返回的json
        {
            if (not contents)
                throw LLM_error{"服务器繁忙，请稍后再试。"};
            string json {encode(CP_UTF8, ptr->prog_enc(), contents)};
            istringstream json_str {json};
            for (string line; ; std::getline(json_str, line)) {
                if (line.empty()) {
                    if (json_str.eof())
                        break;
                    else
                        continue;
                }
                try {
                    string reasoning_content {read_key(line, "reasoning_content")};
                    string content {read_key(line, "content")};
                    bool reasoning = not (reasoning_content=="null");
                    string& selected {(reasoning) ? reasoning_content : content};
                    del_quote(selected);
                    selected = parse(selected);
                    Reasonal_message& func {dynamic_cast<Reasonal_message&>(*ptr)};
                    if (reasoning)
                        func.reason(std::move(selected));
                    else
                        func(std::move(selected));
                }
                catch (Not_found_error) {
                    break;
                }
            }
            return size*nmemb;
        }
    };
    
    class Chat : public LLM {          // Chat类是一个通用模型
    public:
        Chat(string&& url, string&& model, string&& key, function<void(string&&)> func, int code_encode, int prog_encode) : LLM{std::move(url),std::move(model),std::move(key),code_encode,prog_encode}, func{func} { set_call_back(call_back); }
        void get(string&& question) override             // 调用大模型
        {
            Curl::Curl curl {set_curl(question)};
            curl.set_write_func(reinterpret_cast<void*>(*(func_call_back().target<size_t(*)(char*, size_t, size_t, Message_func*)>())));
            Chat_message mfunc {prog_enc(),func};
            curl.set_write_data(&mfunc);
            curl.perform();
            add_history(std::move(question), mfunc.get_ans());
        }
        using LLM::get;
        virtual ~Chat() { }
    private:
        function<void(string&&)> func;
        static size_t call_back(char* contents, size_t size, size_t nmemb, Message_func* ptr)          // 处理网络请求中每次返回的json
        {
            if (not contents)
                throw LLM_error{"服务器繁忙，请稍后再试。"};
            string json {encode(CP_UTF8, ptr->prog_enc(), contents)};
            istringstream json_str {json};
            for (string line; ; std::getline(json_str, line)) {
                if (line.empty()) {
                    if (json_str.eof())
                        break;
                    else
                        continue;
                }
                try {
                    string content {read_key(line, "content")};
                    del_quote(content);
                    content = parse(content);
                    dynamic_cast<Chat_message&>(*ptr)(std::move(content));
                }
                catch (Not_found_error) {
                    break;
                }
            }
            return size*nmemb;
        }
    };
    
    class Fim_base : public Chat {       // Fim_base类是上下文补充模型，deepseek的beta功能，目前尚不稳定
    public:
        Fim_base(string&& key, function<void(string&&)> func, int code_encode =CP_ACP, int prog_encode =CP_ACP) : Chat{"https://api.deepseek.com/beta/completions","deepseek-chat",std::move(key),func,code_encode,prog_encode} { set_call_back(call_back); }
    private:
        static size_t call_back(char* contents, size_t size, size_t nmemb, Message_func* ptr)          // 处理网络请求中每次返回的json
        {
            if (not contents)
                throw LLM_error{"服务器繁忙，请稍后再试。"};
            string json {encode(CP_UTF8, ptr->prog_enc(), contents)};
            istringstream json_str {json};
            for (string line; ; std::getline(json_str, line)) {
                if (line.empty()) {
                    if (json_str.eof())
                        break;
                    else
                        continue;
                }
                try {
                    string content {read_key(line, "text")};
                    del_quote(content);
                    content = parse(content);
                    dynamic_cast<Chat_message&>(*ptr)(std::move(content));
                }
                catch (Not_found_error) {
                    break;
                }
            }
            return size*nmemb;
        }
    };
    
}

#endif

