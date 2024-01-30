### hcpp

可以访问某些资源,这些资源在一些时候访问,会受到干扰

即,使用doh发现合适的ip,然后在进行tls握手时关闭sni扩展

起源于[dev-sidecar](https://github.com/docmirror/dev-sidecar)

#### 来自帮助

[beast](https://github.com/boostorg/beast)

[harddns](https://github.com/stealth/harddns)

### 编译

需要c++20

linux下使用clang>16,在debug下编译

windows使用最新编译器

```shell
    xmake g --proxy_pac=github_mirror.lua

    xmake -rvwD hcpp

    xmake run hcpp
```