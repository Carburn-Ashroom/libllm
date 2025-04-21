#ifndef CURL_HPP
#define CURL_HPP

#include <string>
#include <string_view>
#include <memory>

#include <curl.h>

namespace Curl {             // 该名字空间负责简单封装网络连接库
    
    using std::string;
    using std::string_view;
    using std::unique_ptr;
    using std::nothrow;
    
    struct Network_error {};             // 网络连接异常
    
    class Global_resource {        // Global_resource类是一个全局网络环境初始化状态
    public:
        Global_resource() { curl_global_init(CURL_GLOBAL_DEFAULT); }
        ~Global_resource() { curl_global_cleanup(); }
    };
    
    class Curl {             // Curl类是一个网络连接。注意处理抛出的Network_error异常
    public:
        explicit Curl(string_view url) : url{url}, headers{}
        {
            if (not global_init())
                throw Network_error{};
            ptr = curl_easy_init();
            if (not ptr)
                throw Network_error{};
        }
        void add_header(const string& name, const string& value) { headers = curl_slist_append(headers, (name+": "+value).c_str()); }
        void set_body(string&& json) { body = std::move(json); }
        void set_write_func(void* call_back_func) { curl_easy_setopt(ptr, CURLOPT_WRITEFUNCTION, call_back_func); }
        void set_write_data(void* buffer) { curl_easy_setopt(ptr, CURLOPT_WRITEDATA, buffer); }
        void perform() const             // 执行网络请求
        {
            curl_easy_setopt(ptr, CURLOPT_URL, url.c_str());
            curl_easy_setopt(ptr, CURLOPT_HTTPHEADER, headers);
            if (not body.empty()) {
                curl_easy_setopt(ptr, CURLOPT_POSTFIELDS, body.c_str());
                curl_easy_setopt(ptr, CURLOPT_POSTFIELDSIZE, body.length());
            }
            curl_easy_perform(ptr);
        }
        ~Curl()
        {
            curl_slist_free_all(headers);
            curl_easy_cleanup(ptr);
        }
    private:
        CURL* ptr;
        string url;
        curl_slist* headers;
        string body;
        static const Global_resource* global_init()        // 提供全局网络环境初始化状态
        {
            static unique_ptr<Global_resource> global {new(nothrow) Global_resource};
            return global.get();
        }
    };
    
}

#endif
