FROM alpine:3.5

LABEL maintainer "Yuki Kikuchi <bclswl0827@yahoo.co.jp>"

ADD entrypoint.sh /entrypoint.sh
ADD update.sh /update.sh

RUN apk add --no-cache --virtual --update tzdata \
  && cp /usr/share/zoneinfo/Asia/Tokyo /etc/localtime \
  && apk del --purge curl jq tzdata \
  && rm -rf /var/cache/apk/* \
  && chmod +x /*.sh \
  && echo "0 4 * * * /update.sh" | crontab -

ENTRYPOINT ["sh", "-c", "/entrypoint.sh"]
