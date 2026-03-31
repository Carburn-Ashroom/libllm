/**
 * 实现大模型库的根基
 * 创建者：Carburn Ashroom
 * 2026.3.31
 */

#include "llm_impl.h"

namespace LLM_impl {
    
    void LLM::read_file(const string& file, int file_encode)
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
    
    bool LLM::save_file(const string& file, int file_encode)
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
    
    void LLM::set(string&& property, string&& value, bool quote_value)
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
    
    string LLM::request_body(string_view question) const
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
}
