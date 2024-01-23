// DNS消息格式 https://www.rfc-editor.org/rfc/rfc1035#section-4

// The answer, authority, and additional sections all share the same format
// https://www.rfc-editor.org/rfc/rfc1035#section-4.1.3

// 消息压缩 https://www.rfc-editor.org/rfc/rfc1035#section-4.1.4
// The first two bits are ones.  This allows a pointer to be distinguished
// from a label, since the label must begin with two zero bits because
// labels are restricted to 63 octets or less

// 数据传输顺序
// https://www.rfc-editor.org/rfc/rfc1035#section-2.3.2

// rubyfish.cn 回答例子
// 85 193 129 128 0 1 0 1 0 0 0 0 6 103 105 116 104 117 98 3 99 111 109 0 0 1 0 1 192 12 0 1 0 1 0 0 0 37 0 4 192 30 255 113
// 85 193 129 128 前四位描述
// 0 1 0 1 0 0 0 0 表示各个字段数量QDCOUNT ANCOUNT NSCOUNT  ARCOUNT
// 6 103 105 116 104 117 98 3 99 111 109 0  表示QNAME
// 0 1 0 1 表示QNAME的QTYPE和QCLASS
// 192 12 表示消息压缩,192固定,12表示从消息开始处偏移12位
// 0 1 0 1  TYPE和CLASS
// 0 0 0 37  生存时间TTL
// 0 4  RDLENGTH
// 192 30 255 113   RDATA

// 1.1.1.1回答
// 238 213 129 144 描述
// 0 1 0 1 0 0 0 1 QDCOUNT ANCOUNT NSCOUNT ARCOUNT
// 6 103 105 116 104 117 98 3 99 111 109 0 0 1 0 1      QNAME QTYPE QCLASS
// 6 103 105 116 104 117 98 3 99 111 109 0 0 1 0 1      NAME
// 0 0 0 46     TTL
// 0 4 192 30 255 112    RDLENGTH和RDATA
// 下面是扩展部分 好像来自这里 https://www.rfc-editor.org/rfc/rfc6891
// 0    NAME
// 0 41     TYPE
// 16 0     CLASS
// 0 0 0 0      TTL
// 0 0      RDLEN

// dns.alidns.com 回答
//  224 19 129 144 0 1 0 1 0 0 0 1 6 103 105 116 104 117 98 3 99 111 109 0 0 1 0 1       描述 QDCOUNT ANCOUNT NSCOUNT ARCOUNT QNAME QTYPE QCLASS
//  6 103 105 116 104 117 98 3 99 111 109 0 0 1 0 1  回答部分的 NAME TYPE CLASS
//  0 0 0 17     TTL
//  0 4 20 205 243 166       RDLENGTH RDATA
//  0 0 41 16 0 0 0 0 0 0 0      同上 是rfc6891扩展部分