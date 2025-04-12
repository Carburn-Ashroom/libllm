#ifndef LLM_HPP
#define LLM_HPP

#include <llm_dependence.hpp>

namespace LLM {        // 该名字空间负责封装一些常用的大模型接口
    
    /*
        // 所有大模型都提供的接口：
        class LLM {
        public:
            void read_file(const string& file, int encode =CP_UTF8);             // 从文件中读取对话历史，若文件不存在就抛出Not_found_error异常，若文件格式不对抛出File_format_error异常
            bool save_file(const string& file, int encode =CP_UTF8);             // 保存对话历史到文件中，返回是否保存成功
            void set_system(string&& system);          // 设置系统提示词
            virtual void get(string&& question);             // 调用大模型
            void get(const string& question) { get(string{question}); }
            void add_history(string&& ques, string&& ans);       // 设置历史记录，可以用于训练模型
            const string& get_history(string_view ques ="") const;       // 获取历史记录中某问题的答案，若参数为空字符串则最近一次问题的答案，若未找到则抛出Not_found_error，若历史记录为空则抛出Empty_history_error
            complex<string> get_history(int index) const;          // 获取第index次对话的历史记录，若未找到则抛出Not_found_error，若历史记录为空则抛出Empty_history_error
            void clear_history();          // 清空历史记录
            void set_temperature(double temp);       // 温度
            void set_model(string&& m);    // 有些品牌有多个子模型，在这里设置
            void set(string&& property, string&& value, bool quote_value =false);          // 设定模型的某个调用参数，如果quote_value为true，就用引号括住value。
            static string encode(int from, int to, const char* source);        // 将source从from编码转为to编码。source不能为空指针
            string encode(const char* source) const;             // 将代码编码转为程序编码，等价于encode(code_encode, prog_encode, source)
        protected:private:
            // 省略
        };
        
        // 所有深度思考模型都提供的接口：
        class Reasoner : public LLM {
        public:
            const string& remem_reasoning() const;       // 获取上一次深度思考的内容
            // 继承了LLM的所有public接口
        protected:private:
            // 省略
        };
    
        // 所有通用模型都提供的接口：
        class Chat : public LLM {
        public:
            // 继承了LLM的所有public接口
        };
    */
    
    using LLM_impl::Not_found_error;             // 未找到
    using LLM_impl::File_format_error;           // 文件格式错误，比如user后不是assistant，或assistant前不是user
    using LLM_impl::Empty_history_error;         // 在空历史记录中寻找历史记录的异常
    using LLM_impl::LLM_error;                   // 生成出错
    using namespace LLM_impl;
    
    // R1类是DeepSeek的推理模型，以综合能力强大著称
    // 费用：输入4，输出16，单位元/百万tokens
    class R1 : public Reasoner {
    public:
        R1(string&& key, function<void(string&&, bool)> func, int code_encode =CP_ACP, int prog_encode =CP_ACP) : Reasoner{"https://api.deepseek.com/v1/chat/completions","deepseek-reasoner",std::move(key),func,code_encode,prog_encode} { }
    };
    
    // V3类是DeepSeek的文本模型
    // 费用：R1的一半
    class V3 : public Chat {
    public:
        V3(string&& key, function<void(string&&)> func, int code_encode =CP_ACP, int prog_encode =CP_ACP) : Chat{"https://api.deepseek.com/v1/chat/completions","deepseek-chat",std::move(key),func,code_encode,prog_encode} { }
    };
    
    // Zhipu类是智谱清言的免费模型，是目前唯一免费调用API的模型
    // 费用：0
    class Zhipu : public Chat {
    public:
        Zhipu(string&& key, function<void(string&&)> func, int code_encode =CP_ACP, int prog_encode =CP_ACP) : Chat{"https://open.bigmodel.cn/api/paas/v4/chat/completions","glm-4-flash",std::move(key),func,code_encode,prog_encode} { }
    };
    
    // Qwen类是通义千问的QwQ-32B模型，是价格较为实惠的推理模型
    // 费用：输入2，输出6，单位元/百万tokens
    class Qwen : public Reasoner {
    public:
        Qwen(string&& key, function<void(string&&, bool)> func, int code_encode =CP_ACP, int prog_encode =CP_ACP) : Reasoner{"https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions","qwq-32b",std::move(key),func,code_encode,prog_encode} { }
    };
    
    // Doubao类是豆包的角色扮演模型，拥有不错的角色扮演能力
    // 费用：输入0.4，输出1，单位元/百万tokens
    class Doubao : public Chat {
    public:
        Doubao(string&& key, function<void(string&&)> func, int code_encode =CP_ACP, int prog_encode =CP_ACP) : Chat{"https://ark.cn-beijing.volces.com/api/v3/chat/completions","doubao-1-5-pro-32k-character-250228",std::move(key),func,code_encode,prog_encode} { }
    };
    
    // Polite类是DeepSeek-V3的实例化，是非常有素质的AI
    // 费用：同V3
    class Polite : public V3 {
    public:
        Polite(string&& key, function<void(string&&)> func, int code_encode =CP_ACP, int prog_encode =CP_ACP) : V3{std::move(key),func,code_encode,prog_encode} { self_cultivation(); }
        void get(string_view question, int length)       // 调用大模型
        {
            ostringstream request;
            request << question;
            if (length > 0)
                request << encode("。不少于") << length << encode("字");
            V3::get(request.str());
            self_cultivation();
        }
        void get(string&& question) override { get(question, 1300); }
        using V3::get;
    private:
        void self_cultivation()        // 修身养性
        {
            string prompt {encode("你是一个暴躁老哥，骂人既粗俗又狂野，含妈量极高。请尽情地骂，不用拘束")};
            set_system(string{prompt});
            clear_history();
            add_history(std::move(prompt), encode("我操你妈"));
            add_history(encode("你认识张三吗"), encode("张三，我操你妈"));
            add_history(encode("你认识李四吗"), encode("李四，我操你妈"));
            set_temperature(1.3);
        }
    };
    
    // Fim类是deepseek-v3的beta功能，上下文补充模型，目前尚不稳定
    // 费用：同V3
    class Fim : public Fim_base {
    public:
        Fim(string&& key, function<void(string&&)> func, int code_encode =CP_ACP, int prog_encode =CP_ACP) : Fim_base{std::move(key),func,code_encode,prog_encode} { };
        void set_prefix(string&& prefix) { set("prompt", std::move(prefix), true); }
        void set_suffix(string&& suffix) { set("suffix", std::move(suffix), true); }
        void get() { Fim_base::get(string{}); }
        using Fim_base::get;
    };
    
}

#endif
