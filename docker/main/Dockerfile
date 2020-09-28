FROM debian:buster
LABEL maintainer "Yuki Kikuchi <bclswl0827@yahoo.co.jp>"
ARG DEBIAN_FRONTEND=noninteractive

RUN echo "1.0.0.1 p.sdrotg.com" >> /etc/hosts \
  && sed -i "s/deb.debian.org/mirrors.bfsu.edu.cn/g" /etc/apt/sources.list \
  && sed -i "s/security.debian.org/mirrors.bfsu.edu.cn/g" /etc/apt/sources.list \
  && apt-get update \
  && apt-get install -y make git rsync systemd iptables \
  && git clone https://github.com/bclswl0827/FlyDog_SDR_GPS /root/Beagle_SDR_GPS

RUN cd /root/Beagle_SDR_GPS \
  && make \
  && make install

CMD ["/usr/local/bin/kiwid", "-debian", "10", "-use_spidev", "1", "-bg", "-raspsdr"]
