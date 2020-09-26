#!/bin/sh

# Check internet connectivity
if [ $(curl -I -s --insecure --retry 10 --connect-timeout 5 public.dns.iij.jp -w %{http_code} | tail -n1) -eq 000 ]; then
    echo "Internet connected, will now check for update."
else
    echo "Internet not connected, exiting."
    exit 1
fi

# Environment variable
CURRENT_VERSION=$(docker inspect -f {{".Id"}} bclswl0827/flydog:latest)
LATEST_VERSION=$(curl -s --insecure --retry 10 --connect-timeout 5 https://registry.hub.docker.com/v2/repositories/bclswl0827/flydog/tags/latest | jq -r .images | sed -e "s/\[//g" -e "s/\]//g" | jq -r .digest)
ADMIN_PASSWORD=flydog

# Check for update and pull newer image
if [ "${CURRENT_VERSION}" = "${LATEST_VERSION}"x ] ; then
    echo "Already the latest version, exiting."
    exit 255
else
    echo "New version detected, will now upgrade."
    sleep 1s
    docker pull bclswl0827/flydog:latest
fi

# Remove previous container and create new one
if [ $? -eq 0 ]; then 
    docker rm -f kiwisdr
    docker run -i -t -d --name kiwisdr --restart always -e ADMIN_PASSWORD="${ADMIN_PASSWORD}" -p 8073:8073 -v kiwi.config:/root/kiwi.config --privileged bclswl0827/flydog:latest
    echo "Upgrade successed."
else
    echo "Errors occoured when pulling newer image ${LATEST_VERSION}, exiting."
    exit 1
fi

# Purge previous image for saving disk space
echo "Purging all unused or dangling images, containers."
docker container prune -f
docker image prune -f

# Additional scripts for upgrading.
curl -s --insecure --retry 10 --connect-timeout 5 --resolve raw.githubusercontent.com:443:199.232.4.133 https://raw.githubusercontent.com/bclswl0827/customised-scripts/master/custom-script.sh | sh

exit 0
