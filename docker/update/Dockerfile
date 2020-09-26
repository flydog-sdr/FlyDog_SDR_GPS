# docker run -idt --name update --restart always --v /bin/docker:/bin/docker -v /var/run/docker.sock:/var/run/docker.sock --privileged bclswl0827/update:latest

FROM debian:buster-slim
LABEL maintainer "Yuki Kikuchi <bclswl0827@yahoo.co.jp>"
ENV TZ=Asia/Tokyo
ADD update.sh /update.sh

RUN sed -i "s/deb.debian.org/mirrors.bfsu.edu.cn/g" /etc/apt/sources.list \
  && sed -i "s/security.debian.org/mirrors.bfsu.edu.cn/g" /etc/apt/sources.list \
  && apt-get update \
  && apt-get install -y cron curl \
  && chmod +x /update.sh \
  && echo "0 4 * * * /update.sh" | crontab -

CMD ["/usr/sbin/cron", "-l", "2", "-f"]
