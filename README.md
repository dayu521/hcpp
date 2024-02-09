### hcpp

可以访问某些资源,这些资源在一些时候访问,会受到干扰

即,使用doh发现合适的ip,然后在进行tls握手时关闭sni扩展

起源于[dev-sidecar](https://github.com/docmirror/dev-sidecar)

#### 来自帮助

[beast](https://github.com/boostorg/beast)

[harddns](https://github.com/stealth/harddns)

[openssl-sign-by-ca](https://github.com/zozs/openssl-sign-by-ca/)

### 编译

需要c++20,使用xmake构建

linux下使用clang>16,在debug下编译

windows使用最新编译器

打开终端或`windows terminal`

```shell
    xmake g --proxy_pac=github_mirror.lua

    xmake -rvwD hcpp

    xmake run hcpp
```

#### 运行

当前会在配置文件指定的目录(例如 ~/.config/hcpp/)下生成自签名的ca密钥和ca证书

浏览器需要信任该ca证书

使用curl直接进行测试

```shell
    curl -i -x http://localhost:55555 -v --cacert ~/.config/hcpp/ca.cert.pem  'https://gitee.com' 
```

或者
```shell
    curl -i -x http://localhost:55555 -vk  'https://gitee.com' 
```
