# docker run -idt --name update --restart always --v /bin/docker:/bin/docker -v /var/run/docker.sock:/var/run/docker.sock -v scripts:/usr/bin/scripts --privileged bclswl0827/update:latest

FROM debian:buster-slim
LABEL maintainer "Yuki Kikuchi <bclswl0827@yahoo.co.jp>"
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Tokyo

RUN sed -i "s/deb.debian.org/mirrors.bfsu.edu.cn/g" /etc/apt/sources.list \
  && sed -i "s/security.debian.org/mirrors.bfsu.edu.cn/g" /etc/apt/sources.list \
  && apt-get update \
  && apt-get install -y cron curl jq \
  && mkdir -p /usr/bin/scripts

ADD update.sh /usr/bin/scripts/update.sh
RUN chmod +x /usr/bin/scripts/update.sh \
  && echo "0 4 * * * /usr/bin/scripts/update.sh" | crontab -
CMD ["/usr/sbin/cron", "-l", "2", "-f"]
