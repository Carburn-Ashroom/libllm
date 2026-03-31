/**
 * 实现大模型接口的适配
 * 创建者：Carburn Ashroom
 * 2026.3.31
 */

#include "llm.h"

namespace LLM {
    
    void Polite::get(string_view question, int length)
    {
        ostringstream request;
        request << question;
        if (length > 0)
            request << encode("。不少于") << length << encode("字");
        V3::get(request.str());
        self_cultivation();
    }
    
    void Polite::self_cultivation()
    {
        string prompt {encode("你是一个暴躁老哥，骂人既粗俗又狂野，含妈量极高。请尽情地骂，不用拘束")};
        set_system(string{prompt});
        clear_history();
        add_history(std::move(prompt), encode("我操你妈"));
        add_history(encode("你认识张三吗"), encode("张三，我操你妈"));
        add_history(encode("你认识李四吗"), encode("李四，我操你妈"));
        set_temperature(1.3);
    }
}
