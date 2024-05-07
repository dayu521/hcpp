# 使用opensuse/tumbleweed作为基础镜像  
FROM opensuse/tumbleweed  

RUN zypper mr -da
  
# 替换zypper的源为国内镜像（清华大学）  
# RUN zypper ar -fcg 'https://mirrors.ustc.edu.cn/opensuse/tumbleweed/repo/oss' USTC:OSS 
# RUN zypper ar -fcg 'https://mirrors.ustc.edu.cn/opensuse/tumbleweed/repo/non-oss' USTC:NON-OSS  
# RUN zypper ar -fcg 'https://mirrors.ustc.edu.cn/opensuse/update/tumbleweed' USTC:UPDATE

# 定义一个环境变量，国内镜像
ARG DEFAULT_CN_MIRROR=https://mirrors.ustc.edu.cn

ENV USE_CN_MIRROR=false;

# 根据环境变量设置镜像源  
RUN if [ $USE_CN_MIRROR == true ]; then \  

        echo "Using China mirror..." && \  

        zypper ar -fcg '${CN_MIRROR:-$DEFAULT_CN_MIRROR}/opensuse/tumbleweed/repo/oss/' OSS && \  

        zypper ar -fcg '${CN_MIRROR:-$DEFAULT_CN_MIRROR}/opensuse/tumbleweed/repo/non-oss' NON-OSS && \  

        zypper ar -fcg '${CN_MIRROR:-$DEFAULT_CN_MIRROR}/opensuse/update/tumbleweed' UPDATE; \  

    else \  

        echo "Using default repositories..."; \  

    fi  
  
# 刷新
RUN zypper refresh 
  
# 安装基本依赖 
RUN zypper install -y bash-completion gawk unzip curl

# 安装hcpp依赖
RUN zypper install -y clang spdlog-devel asio-devel openssl

# 安装xmake依赖
RUN zypper install -y readline-devel git
RUN zypper install -y -t pattern devel_C_C++

# 添加dev用户（出于安全考虑，不推荐在Dockerfile中硬编码密码）  
RUN useradd -ms /bin/bash dev  

# 切换到dev用户  
USER dev

# 安装xmake  https://xmake.io/#/zh-cn/guide/installation
RUN curl -fsSL https://xmake.io/shget.text | bash
  
# 如果需要，可以设置环境变量  
# ENV VARIABLE_NAME=value  
  
# 定义容器启动时运行的命令（可选）  
# CMD ["executable","param1","param2"]  
  
# 容器启动时以非root用户运行（可选，但更安全）  
# USER dev
