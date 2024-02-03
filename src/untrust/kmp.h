#ifndef SRC_UNTRUST_KMP
#define SRC_UNTRUST_KMP

#include <string>
#include <vector>

namespace hcpp::untrust
{
    /// 由chat gpt 生成
    using std::vector, std::string, std::string_view;

    class KMP
    {
    private:
        vector<int> lps;
        std::string pattern_;

        int j = 0;
        int matchIndex = -1;
        std::size_t match_len_ = 0;

        void computeLPSArray()
        {
            int len = 0;
            int i = 1;
            int m = pattern_.size();

            lps.resize(m, 0);
            lps[0] = 0;

            while (i < m)
            {
                if (pattern_[i] == pattern_[len])
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
        KMP(string pattern) : pattern_(pattern)
        {
            computeLPSArray();
        }

        int search(string_view text)
        {
            int n = text.size();
            int m = pattern_.size();
            int i = 0;

            while (i < n)
            {
                if (pattern_[j] == text[i])
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
                    return i - j + match_len_; // 返回匹配的索引
                }
                else if (i < n && pattern_[j] != text[i])
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
            auto r = matchIndex + match_len_; // 未找到匹配，返回已经匹配的索引
            match_len_ += n;
            return r;
        }
    };
} // namespace hcpp::untrust

#endif /* SRC_UNTRUST_KMP */
