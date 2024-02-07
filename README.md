### hcpp

可以访问某些资源,这些资源在一些时候访问,会受到干扰

即,使用doh发现合适的ip,然后在进行tls握手时关闭sni扩展

起源于[dev-sidecar](https://github.com/docmirror/dev-sidecar)

#### 来自帮助

[beast](https://github.com/boostorg/beast)

[harddns](https://github.com/stealth/harddns)

### 编译

需要c++20,使用xmake构建

linux下使用clang>16,在debug下编译

windows使用最新编译器

打开终端或`wt`

```shell
    xmake g --proxy_pac=github_mirror.lua

    xmake -rvwD hcpp

    xmake run hcpp
```

#### 例子

当前测试了三个主机,`github.com`,`gitee.com`,`www.baidu.com`.这三者由mitm代理.使用下面的自签名的`ca证书`,签名了服务的证书.服务的证书包含了上述三个主机名

其余主机都是正常的http proxy或者http tunnel代理

[ca证书](doc/cert/ca.crt),可以导入浏览器

下面的证书和密钥需要放到程序运行目录

- 使用的[证书](doc/cert/server.crt.pem),由密钥生成,被ca签名

- 使用的[密钥](doc/cert/server.key.pem)