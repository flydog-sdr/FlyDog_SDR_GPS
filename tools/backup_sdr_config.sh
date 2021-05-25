#!/usr/bin/env bash

# Define basic variables
NETWORK_CHECK_RESPONSE="405"
DIR_TMP="$(mktemp -d)"

hello_message() {
  echo -e "This script will backup your configuration and generate a"
  echo -e "download URL so that you can download them to local PC."
}

check_internet_connectivity() {
  if [[ $(curl -I -s transfer.sh -w %{http_code} | tail -n1) != ${NETWORK_CHECK_RESPONSE} ]]; then
    echo "Internet is not connected, exiting..."
    exit 1
  fi
}

archiving_configuration() {
  cd /root
  tar -zcf ${DIR_TMP}/config.tar.gz kiwi.config
}

upload_configuration() {
  echo -e "Uploading your configuration..."
  curl --silent --upload-file ${DIR_TMP}/config.tar.gz https://transfer.sh/config.tar.gz
  echo -e "\n\nSuccess! You can download your configuration via the link above."
  rm -rf ${DIR_TMP}
}

main() {
  hello_message
  check_internet_connectivity
  archiving_configuration
  upload_configuration
}
main "$@"