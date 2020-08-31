FROM debian:buster
LABEL maintainer "Yuki Kikuchi <bclswl0827@yahoo.co.jp>"
ARG DEBIAN_FRONTEND=noninteractive

RUN sed -i "s/deb.debian.org/mirrors.bfsu.edu.cn/g" /etc/apt/sources.list \
  && sed -i "s/security.debian.org/mirrors.bfsu.edu.cn/g" /etc/apt/sources.list \
  && apt-get update \
  && apt-get install -y make git rsync systemd iptables

RUN git clone https://github.com/bclswl0827/FlyDog_SDR_GPS /root/Beagle_SDR_GPS \
  && cd /root/Beagle_SDR_GPS \
  && make \
  && make install

RUN apt-get remove --purge -y git make rsync systemd \
  && apt-get --purge -y autoremove

CMD [ "/usr/local/bin/kiwid", "-debian", "10", "-use_spidev", "1", "-bg" ]
