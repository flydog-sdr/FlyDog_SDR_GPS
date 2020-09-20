FROM alpine:3.5

LABEL maintainer "Yuki Kikuchi <bclswl0827@yahoo.co.jp>"

ADD update.sh /update.sh
RUN apk add --no-cache --virtual --update curl jq tzdata \
  && cp /usr/share/zoneinfo/Asia/Tokyo /etc/localtime \
  && rm -rf /var/cache/apk/* \
  && chmod +x /*.sh \
  && echo "0 4 * * * /update.sh" | crontab -

CMD ["/sbin/crond", "-l", "2", "-f"]
