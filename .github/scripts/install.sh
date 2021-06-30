#!/usr/bin/env bash
curl -fsSL https://deb.nodesource.com/setup_12.x | sudo -E bash -
apt-get update -y
apt-get install -y build-essential uuid-dev libssl-dev git curl nodejs

# Build hiredis
mkdir -p "$HOME/src"
cd "$HOME/src"
git clone https://github.com/redis/hiredis
cd hiredis && make && make install

# Install Sonar
curl https://sonarcloud.io/static/cpp/build-wrapper-linux-x86.zip -o /tmp/build-wrapper.zip
curl https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-4.4.0.2170-linux.zip -o /tmp/sonar-scanner.zip
cd "/home/runner/"
unzip /tmp/build-wrapper.zip
unzip /tmp/sonar-scanner.zip
rm -rf /tmp/build-wrapper.zip /tmp/sonar-scanner.zip
