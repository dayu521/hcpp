#ifndef SRC_UNTRUST_KMP
#define SRC_UNTRUST_KMP

#include <string>
#include <vector>

namespace hcpp::untrust
{
    /// 由chat gpt 生成
    using std::vector, std::string,std::string_view;

    class KMP
    {
    private:
        vector<int> lps;

        void computeLPSArray(string pattern)
        {
            int len = 0;
            int i = 1;
            int m = pattern.size();

            lps.resize(m, 0);
            lps[0] = 0;

            while (i < m)
            {
                if (pattern[i] == pattern[len])
                {
                    len++;
                    lps[i] = len;
                    i++;
                }
                else
                {
                    if (len != 0)
                    {
                        len = lps[len - 1];
                    }
                    else
                    {
                        lps[i] = 0;
                        i++;
                    }
                }
            }
        }

    public:
        KMP(string pattern)
        {
            computeLPSArray(pattern);
        }

        int search(string_view text, string_view pattern)
        {
            int n = text.size();
            int m = pattern.size();

            int i = 0;
            int j = 0;
            int matchIndex = -1;

            while (i < n)
            {
                if (pattern[j] == text[i])
                {
                    if (j == 0)
                    {
                        matchIndex = i; // 如果是匹配的起始位置，记录匹配的索引
                    }
                    j++;
                    i++;
                }

                if (j == m)
                {
                    return i - j; // 返回匹配的索引
                }
                else if (i < n && pattern[j] != text[i])
                {
                    if (j != 0)
                    {
                        j = lps[j - 1];
                    }
                    else
                    {
                        i++;
                    }
                }
            }
            return matchIndex; // 未找到匹配，返回已经匹配的索引
        }
    };
} // namespace hcpp::untrust

#endif /* SRC_UNTRUST_KMP */
