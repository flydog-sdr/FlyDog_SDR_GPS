#!/usr/bin/env bash

# Define basic variables
DIR_TMP="$(mktemp -d)"

# Define font colour
Green_font_prefix="\033[32m"
Red_font_prefix="\033[31m"
Green_background_prefix="\033[42;37m"
Red_background_prefix="\033[41;37m"
Font_color_suffix="\033[0m"

# Define log colour
INFO="${Green_font_prefix}[INFO]${Font_color_suffix}"
ERROR="${Red_font_prefix}[ERROR]${Font_color_suffix}"
TIP="${Green_font_prefix}[TIP]${Font_color_suffix}"

# Check connectivity
check_net() {
  if [[ "$(curl -fsSIL fast.com -w %{http_code} | tail -n1)" != "200" ]]; then
    echo -e "${ERROR} Internet is not connected, exiting..."
    err=1
    sleep 3s
    exit 1
  fi
}

archive_conf() {
  echo -e "${INFO} Compressing files, please wait..."
  cp -r /root/kiwi.config ${DIR_TMP}
  rm -rf ${DIR_TMP}/kiwi.config/_VER
  cd ${DIR_TMP}
  tar -zcf ${DIR_TMP}/config.tar.gz kiwi.config
}

upload_conf() {
  echo -e "${INFO} Uploading configuration, please wait..."
  curl -H "Max-Downloads: 1" -H "Max-Days: 1" -fsSL transfer.sh/config.tar.gz --upload-file ${DIR_TMP}/config.tar.gz -o ${DIR_TMP}/url.txt
  if [[ "$?" != "0" ]]; then
    echo -e "${ERROR} Failed to upload archive, exiting..."
    err=15
    sleep 3s
    exit 1
  else
    DOWNLOAD_URL="$(cat ${DIR_TMP}/url.txt)"
    echo; echo -e "${TIP} Please download the files via the following link:"
    echo -e "${TIP} ${DOWNLOAD_URL}"
    echo; echo -e "${INFO} Backup finished, exiting..."
    sleep 3s
  fi
}

main() {
  check_net
  archive_conf
  upload_conf
  rm -rf ${DIR_TMP}
}
main "$@"