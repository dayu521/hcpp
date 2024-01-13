// Conforming CAs MUST support key identifiers (sections 4.2.1.1 and
//    4.2.1.2), basic constraints (section 4.2.1.10), key usage (section
//    4.2.1.3), and certificate policies (section 4.2.1.5) extensions.

// openssl req -newkey rsa:2048 -keyout domain.key -x509 -days 365 -out domain.crt
// 上面命令创建的证书只包含下述三个
// Basic Constraints 表示当前证书绑定的实体否是ca,要想创建ca,则必须包含他
// Subject Key Identifier CA证书必须包含
// Authority Key Identifier 必须包含
// https://www.rfc-editor.org/rfc/rfc3280#section-4.2.1.10

// ASN.1 DER(distinguished encoding rules)
//https://www.rfc-editor.org/rfc/rfc5280#section-4.1

// Certificate Revocation List (CRL) 

// NID https://www.openssl.org/docs/manmaster/man3/X509V3_EXT_d2i.html

// ASN1_OCTET_STRING_new
// https://www.openssl.org/docs/manmaster/man3/ASN1_STRING_type_new.html

// BASIC_CONSTRAINTS_new
// https://www.openssl.org/docs/manmaster/man3/BASIC_CONSTRAINTS_new.html

// i2d和d2i开头风格的函数
// https://www.openssl.org/docs/manmaster/man3/d2i_ASN1_OCTET_STRING.html

// 添加扩展
// https://www.openssl.org/docs/manmaster/man3/X509_add1_ext_i2d.html

// 添加扩展的例子
// https://github.com/openssl/openssl/issues/11706

// 证书生成
// https://github.com/zozs/openssl-sign-by-ca

// 先创建假证书
// 使用假ca对假证书签名